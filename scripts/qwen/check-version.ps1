# Check Qwen Code version and whether this repo already published qwen.exe.

$ErrorActionPreference = "Stop"

$LatestVersionUrl = "https://qwen-code-assets.oss-cn-hangzhou.aliyuncs.com/releases/qwen-code/latest/VERSION"
$ReleaseTagPrefix = "qwencode-v"
$RequiredLauncherRevision = "2"

function Get-ResponseText {
    param(
        [string]$Url
    )

    $response = Invoke-WebRequest -Uri $Url -UseBasicParsing
    $content = $response.Content

    if ($content -is [byte[]]) {
        return [System.Text.Encoding]::UTF8.GetString($content)
    }

    return [string]$content
}

function Get-LatestVersion {
    try {
        $version = Get-ResponseText -Url $LatestVersionUrl
        $version = $version.Trim().Trim([char]0xFEFF)
        if ($version.StartsWith("v")) {
            $version = $version.Substring(1)
        }
        return $version
    }
    catch {
        Write-Error "Failed to fetch latest Qwen Code version: $_"
        exit 1
    }
}

function Get-ReleaseAssetNames {
    param(
        [string]$Tag
    )

    $releaseJson = gh release view "$Tag" --json assets 2>$null
    if (-not $?) {
        return @()
    }

    $release = $releaseJson | ConvertFrom-Json
    return @($release.assets | ForEach-Object { $_.name })
}

function Get-ReleaseLauncherRevision {
    param(
        [string]$Tag
    )

    $assets = Get-ReleaseAssetNames -Tag $Tag
    if ($assets -notcontains "launcher.revision") {
        return $null
    }

    $tempFile = Join-Path $env:RUNNER_TEMP ([System.IO.Path]::GetRandomFileName())
    try {
        gh release download "$Tag" -p "launcher.revision" -O $tempFile 2>$null
        if (-not $?) {
            return $null
        }
        return (Get-Content $tempFile -Raw).Trim()
    }
    finally {
        if (Test-Path $tempFile) {
            Remove-Item $tempFile -Force
        }
    }
}

function Test-PortableReleaseComplete {
    param(
        [string]$Version
    )

    $tag = "$ReleaseTagPrefix$Version"
    $assets = Get-ReleaseAssetNames -Tag $tag

    if ($assets.Count -eq 0) {
        return $false
    }

    if ($assets -notcontains "qwen.exe") {
        return $false
    }

    $launcherRevision = Get-ReleaseLauncherRevision -Tag $tag
    if ($launcherRevision -ne $RequiredLauncherRevision) {
        Write-Host "Release $tag launcher revision is '$launcherRevision', expected '$RequiredLauncherRevision'"
        return $false
    }

    return $true
}

$latestVersion = Get-LatestVersion
Write-Output "Latest Qwen Code version: $latestVersion"

if (Test-PortableReleaseComplete -Version $latestVersion) {
    Write-Output "Version $latestVersion already exists as release $ReleaseTagPrefix$latestVersion with qwen.exe"
    exit 1
}

$tag = "$ReleaseTagPrefix$latestVersion"
$existingAssets = Get-ReleaseAssetNames -Tag $tag
if ($existingAssets.Count -gt 0) {
    Write-Output "Release $tag exists but needs republish (missing qwen.exe or outdated launcher)"
} else {
    Write-Output "New Qwen Code version detected: $latestVersion"
}

Write-Output $latestVersion
exit 0
