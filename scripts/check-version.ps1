# Check Version Script
# Retrieves the latest Claude Code version and checks if it already exists

$ErrorActionPreference = "Stop"

# Version API endpoint
$VersionApiUrl = "https://storage.googleapis.com/claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/claude-code-releases/latest"

function Get-LatestVersion {
    try {
        $response = Invoke-RestMethod -Uri $VersionApiUrl -Method Get
        return $response.Trim()
    }
    catch {
        Write-Error "Failed to fetch latest version: $_"
        exit 1
    }
}

function Test-VersionExists {
    param(
        [string]$Version
    )

    # Check if release tag already exists using GitHub CLI
    $tag = "v$Version"
    $result = gh release view "$tag" 2>$null

    return $?
}

# Main execution
$latestVersion = Get-LatestVersion
Write-Output "Latest version: $latestVersion"

if (Test-VersionExists -Version $latestVersion) {
    Write-Output "Version $latestVersion already exists as a release"
    exit 1  # Return non-zero to indicate no new version
} else {
    Write-Output "New version detected: $latestVersion"
    Write-Output $latestVersion  # Output version for other scripts to use
    exit 0
}
