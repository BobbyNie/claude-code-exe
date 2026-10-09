# Check all upstream versions and whether this repo already published one bundle release.

param(
    [switch]$NoExit,

    [Parameter(Mandatory = $false)]
    [string]$Repo = ""
)

$ErrorActionPreference = "Stop"

$ClaudeVersionUrl = "https://storage.googleapis.com/claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/claude-code-releases/latest"
$QwenVersionUrl = "https://qwen-code-assets.oss-cn-hangzhou.aliyuncs.com/releases/qwen-code/latest/VERSION"
$CodexAppProductUrl = "https://displaycatalog.mp.microsoft.com/v7.0/products/9PLM9XGG6VKS?market=US&languages=en-US&fieldsTemplate=details"
$CodexCliLatestReleaseUrl = "https://api.github.com/repos/openai/codex/releases/latest"

if (-not $Repo -and $env:GITHUB_REPOSITORY) {
    $Repo = $env:GITHUB_REPOSITORY
}

function Get-RepoName {
    if ($Repo) {
        return $Repo
    }

    return (gh repo view --json nameWithOwner -q .nameWithOwner)
}

function Get-ResponseText {
    param([string]$Url)

    $response = Invoke-WebRequest -Uri $Url -UseBasicParsing
    $content = $response.Content

    if ($content -is [byte[]]) {
        return [System.Text.Encoding]::UTF8.GetString($content)
    }

    return [string]$content
}

function Invoke-JsonRequest {
    param([string]$Url)

    $headers = @{
        "Accept" = "application/json"
        "User-Agent" = "claude-code-exe-release-bot"
    }

    if ($env:GITHUB_TOKEN -and $Url.StartsWith("https://api.github.com/")) {
        $headers["Authorization"] = "Bearer $env:GITHUB_TOKEN"
        $headers["X-GitHub-Api-Version"] = "2022-11-28"
    }

    return Invoke-RestMethod -Uri $Url -Method Get -Headers $headers
}

function Convert-DateVersion {
    param([string]$Value)

    try {
        $date = [DateTimeOffset]::Parse($Value)
        return $date.UtcDateTime.ToString("yyyyMMddHHmmss")
    }
    catch {
        return ($Value -replace "[^0-9A-Za-z]+", "")
    }
}

function Get-ClaudeLatestVersion {
    $version = Get-ResponseText -Url $ClaudeVersionUrl
    return $version.Trim().Trim([char]0xFEFF)
}

function Get-QwenLatestVersion {
    $version = Get-ResponseText -Url $QwenVersionUrl
    $version = $version.Trim().Trim([char]0xFEFF)
    if ($version.StartsWith("v")) {
        $version = $version.Substring(1)
    }
    return $version
}

function Get-CodexAppPackageInfo {
    $product = Invoke-JsonRequest -Url $CodexAppProductUrl
    $packages = @()

    foreach ($availability in @($product.Product.DisplaySkuAvailabilities)) {
        if ($availability.Sku -and $availability.Sku.Properties -and $availability.Sku.Properties.Packages) {
            $packages += @($availability.Sku.Properties.Packages)
        }
    }

    $package = $packages |
        Where-Object {
            $_.PackageFamilyName -eq "OpenAI.Codex_2p2nqsd0c76g0" -and
            $_.PackageFullName -match "_x64__" -and
            $_.PackageFormat -eq "Msix"
        } |
        Select-Object -First 1

    if (-not $package) {
        throw "Could not find x64 MSIX package metadata for Codex App"
    }

    $packageVersion = $null
    if ($package.PackageFullName -match "^OpenAI\.Codex_([^_]+)_") {
        $packageVersion = $Matches[1]
    }

    if (-not $packageVersion) {
        $revision = $product.Product.Properties.RevisionId
        if (-not $revision) {
            $revision = $product.Product.LastModifiedDate
        }
        $packageVersion = "store-$(Convert-DateVersion -Value $revision)"
    }

    return [pscustomobject]@{
        Version = $packageVersion
        PackageFormat = $package.PackageFormat
        PackageFullName = $package.PackageFullName
        MaxDownloadSizeInBytes = $package.MaxDownloadSizeInBytes
    }
}

function Get-CodexAppLatestVersion {
    $package = Get-CodexAppPackageInfo
    return $package.Version
}

function Get-CodexCliLatestVersion {
    $release = Invoke-JsonRequest -Url $CodexCliLatestReleaseUrl
    $tag = [string]$release.tag_name

    if ($tag -match "^rust-v(.+)$") {
        return $Matches[1]
    }

    if ($release.name) {
        return [string]$release.name
    }

    return $tag
}

function New-BundleReleaseTag {
    param(
        [string]$ClaudeVersion,
        [string]$QwenVersion,
        [string]$CodexAppVersion,
        [string]$CodexCliVersion
    )

    return "ai-tools-claude-$ClaudeVersion-qwen-$QwenVersion-codex-app-$CodexAppVersion-codex-cli-$CodexCliVersion-qwen-launcher-r4"
}

function Get-BundleRequiredAssets {
    return @(
        "claude.exe",
        "SHA256SUMS.txt",
        "claude-wrapper.bat",
        "qwen-wrapper.bat",
        "codex-wrapper.bat",
        "qwen-launcher.revision",
        "qwen.exe",
        "Codex.msix",
        "codex.exe",
        "README.txt"
    )
}

function Get-ReleaseAssetNames {
    param(
        [string]$Tag,
        [string]$RepoName = $(Get-RepoName)
    )

    $releaseJson = gh release view "$Tag" --repo "$RepoName" --json assets,isDraft 2>$null
    if (-not $?) {
        return @()
    }

    $release = $releaseJson | ConvertFrom-Json
    if ($release.isDraft) { return @() }
    return @($release.assets | ForEach-Object { $_.name })
}

function Test-BundleReleaseComplete {
    param(
        [string]$Tag,
        [string[]]$RequiredAssets = $(Get-BundleRequiredAssets),
        [string]$RepoName = $(Get-RepoName)
    )

    $assets = Get-ReleaseAssetNames -Tag $Tag -RepoName $RepoName
    if ($assets.Count -eq 0) {
        return $false
    }

    foreach ($required in $RequiredAssets) {
        if ($assets -notcontains $required) {
            Write-Host "Release $Tag is missing asset: $required"
            return $false
        }
    }

    return $true
}

function Get-LatestBundleInfo {
    $claudeVersion = Get-ClaudeLatestVersion
    $qwenVersion = Get-QwenLatestVersion
    $codexAppVersion = Get-CodexAppLatestVersion
    $codexCliVersion = Get-CodexCliLatestVersion
    $releaseTag = New-BundleReleaseTag `
        -ClaudeVersion $claudeVersion `
        -QwenVersion $qwenVersion `
        -CodexAppVersion $codexAppVersion `
        -CodexCliVersion $codexCliVersion

    return [pscustomobject]@{
        ReleaseTag = $releaseTag
        ClaudeVersion = $claudeVersion
        QwenVersion = $qwenVersion
        CodexAppVersion = $codexAppVersion
        CodexCliVersion = $codexCliVersion
    }
}

if ($NoExit) {
    return
}

$bundle = Get-LatestBundleInfo

Write-Output "Latest Claude Code version: $($bundle.ClaudeVersion)"
Write-Output "Latest Qwen Code version: $($bundle.QwenVersion)"
Write-Output "Latest Codex App version: $($bundle.CodexAppVersion)"
Write-Output "Latest Codex CLI version: $($bundle.CodexCliVersion)"
Write-Output "Expected bundle release tag: $($bundle.ReleaseTag)"

Write-Output "release_tag=$($bundle.ReleaseTag)"
Write-Output "claude_version=$($bundle.ClaudeVersion)"
Write-Output "qwen_version=$($bundle.QwenVersion)"
Write-Output "codex_app_version=$($bundle.CodexAppVersion)"
Write-Output "codex_cli_version=$($bundle.CodexCliVersion)"

if (Test-BundleReleaseComplete -Tag $bundle.ReleaseTag) {
    Write-Output "Bundle release $($bundle.ReleaseTag) already exists with required assets"
    exit 1
}

Write-Output "New bundle release needed: $($bundle.ReleaseTag)"
exit 0
