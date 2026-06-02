# Validate the published bundle GitHub Release against official latest versions.

$ErrorActionPreference = "Stop"

. "$PSScriptRoot\check-all-versions.ps1" -NoExit

$Repo = if ($env:GITHUB_REPOSITORY) { $env:GITHUB_REPOSITORY } else {
    gh repo view --json nameWithOwner -q .nameWithOwner
}

Write-Output "=== GitHub Bundle Release Check ($Repo) ==="

$bundle = Get-LatestBundleInfo
$requiredAssets = @(
    "claude.exe",
    "qwen.exe",
    "Codex.msix",
    "codex.exe",
    "README.txt"
)

Write-Output "[Claude] Official latest: $($bundle.ClaudeVersion)"
Write-Output "[Qwen] Official latest: $($bundle.QwenVersion)"
Write-Output "[Codex App] Store latest: $($bundle.CodexAppVersion)"
Write-Output "[Codex CLI] Official latest: $($bundle.CodexCliVersion)"
Write-Output "[Bundle] Expected tag: $($bundle.ReleaseTag)"

if (-not (Test-BundleReleaseComplete -Tag $bundle.ReleaseTag -RequiredAssets $requiredAssets -RepoName $Repo)) {
    Write-Output "FAILED: bundle release $($bundle.ReleaseTag) is missing or incomplete"
    exit 1
}

Write-Output "PASSED: latest upstream versions are published in one GitHub Release"
exit 0
