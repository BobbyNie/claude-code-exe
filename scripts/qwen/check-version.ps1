# Check Qwen Code version and whether this repo already published it.

$ErrorActionPreference = "Stop"

$LatestVersionUrl = "https://qwen-code-assets.oss-cn-hangzhou.aliyuncs.com/releases/qwen-code/latest/VERSION"
$ReleaseTagPrefix = "qwencode-v"

function Get-LatestVersion {
    try {
        $response = Invoke-WebRequest -Uri $LatestVersionUrl -UseBasicParsing
        $version = $response.Content.Trim().Trim([char]0xFEFF)
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

function Test-VersionExists {
    param(
        [string]$Version
    )

    $tag = "$ReleaseTagPrefix$Version"
    $null = gh release view "$tag" 2>$null
    return $?
}

$latestVersion = Get-LatestVersion
Write-Output "Latest Qwen Code version: $latestVersion"

if (Test-VersionExists -Version $latestVersion) {
    Write-Output "Version $latestVersion already exists as release $ReleaseTagPrefix$latestVersion"
    exit 1
}

Write-Output "New Qwen Code version detected: $latestVersion"
Write-Output $latestVersion
exit 0
