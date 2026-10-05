$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../../scripts/check-all-versions.ps1" -NoExit
function gh {
    $global:LASTEXITCODE = 0
    return (@{isDraft=$true; assets=@(Get-BundleRequiredAssets | ForEach-Object { @{name=$_} })} | ConvertTo-Json -Depth 4)
}
if (Test-BundleReleaseComplete -Tag fixture -RepoName fixture) {
    throw 'A draft must not suppress publication even when all files are uploaded'
}
$script:assets = @('claude.exe', 'qwen.exe', 'Codex.msix', 'codex.exe', 'README.txt')
function Get-ReleaseAssetNames { param($Tag, $RepoName) return $script:assets }
if (Test-BundleReleaseComplete -Tag fixture -RepoName fixture) {
    throw 'Bundle missing wrappers and checksums must be incomplete'
}
$script:assets = @(Get-BundleRequiredAssets)
if ($script:assets -contains 'ccode.exe' -or $script:assets -contains 'ccode-provenance.json') { throw 'ccode must not be required' }
if ($script:assets.Count -ne 10) { throw 'Four-product bundle must require ten assets' }
if (-not (Test-BundleReleaseComplete -Tag fixture -RepoName fixture)) {
    throw 'Complete four-product bundle must be accepted'
}
foreach ($missing in @(Get-BundleRequiredAssets)) {
    $script:assets = @(Get-BundleRequiredAssets | Where-Object { $_ -ne $missing })
    if (Test-BundleReleaseComplete -Tag fixture -RepoName fixture) {
        throw "Missing required asset accepted: $missing"
    }
}
Write-Output 'PASS: bundle completeness'
