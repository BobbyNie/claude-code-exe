# Validate published GitHub Releases against official latest versions.

$ErrorActionPreference = "Stop"

$Repo = if ($env:GITHUB_REPOSITORY) { $env:GITHUB_REPOSITORY } else {
    gh repo view --json nameWithOwner -q .nameWithOwner
}

$failures = @()

function Get-ResponseText {
    param([string]$Url)

    $response = Invoke-WebRequest -Uri $Url -UseBasicParsing
    $content = $response.Content

    if ($content -is [byte[]]) {
        return [System.Text.Encoding]::UTF8.GetString($content)
    }

    return [string]$content
}

function Test-ReleaseAssets {
    param(
        [string]$Tag,
        [string[]]$RequiredAssets
    )

    $releaseJson = gh release view $Tag --repo $Repo --json assets 2>$null
    if (-not $?) {
        return "Release tag $Tag not found"
    }

    $release = $releaseJson | ConvertFrom-Json
    $assetNames = @($release.assets | ForEach-Object { $_.name })

    foreach ($required in $RequiredAssets) {
        if ($assetNames -notcontains $required) {
            return "Release $Tag is missing asset: $required"
        }
    }

    return $null
}

Write-Output "=== GitHub Release Check ($Repo) ==="

# Claude Code
try {
    $claudeLatest = (Invoke-RestMethod -Uri "https://storage.googleapis.com/claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/claude-code-releases/latest").Trim()
    $claudeTag = "v$claudeLatest"
    Write-Output "[Claude] Official latest: $claudeLatest | Expected tag: $claudeTag"

    $claudeError = Test-ReleaseAssets -Tag $claudeTag -RequiredAssets @("claude.exe", "wrapper.bat", "README.txt")
    if ($claudeError) {
        $failures += "[Claude] $claudeError"
        Write-Output "[Claude] FAIL: $claudeError"
    } else {
        Write-Output "[Claude] OK: release $claudeTag exists with required assets"
    }
}
catch {
    $failures += "[Claude] Failed to check: $_"
    Write-Output "[Claude] FAIL: $_"
}

# Qwen Code
try {
    $qwenRaw = Get-ResponseText -Url "https://qwen-code-assets.oss-cn-hangzhou.aliyuncs.com/releases/qwen-code/latest/VERSION"
    $qwenLatest = $qwenRaw.Trim().Trim([char]0xFEFF)
    if ($qwenLatest.StartsWith("v")) {
        $qwenLatest = $qwenLatest.Substring(1)
    }
    $qwenTag = "qwencode-v$qwenLatest"
    Write-Output "[Qwen] Official latest: $qwenLatest | Expected tag: $qwenTag"

    $qwenError = Test-ReleaseAssets -Tag $qwenTag -RequiredAssets @("qwen.exe", "wrapper.bat", "README.txt")
    if ($qwenError) {
        $failures += "[Qwen] $qwenError"
        Write-Output "[Qwen] FAIL: $qwenError"
    } else {
        Write-Output "[Qwen] OK: release $qwenTag exists with required assets"
    }
}
catch {
    $failures += "[Qwen] Failed to check: $_"
    Write-Output "[Qwen] FAIL: $_"
}

Write-Output "=== Summary ==="
if ($failures.Count -gt 0) {
    Write-Output "FAILED ($($failures.Count) issue(s)):"
    $failures | ForEach-Object { Write-Output "  - $_" }
    exit 1
}

Write-Output "PASSED: all official latest versions are published on GitHub Releases"
exit 0
