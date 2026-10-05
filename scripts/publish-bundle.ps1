# Publish only fully tested local assets; preserve existing releases during repair.
param(
    [Parameter(Mandatory = $true)][string]$Tag,
    [Parameter(Mandatory = $true)][string]$Repo,
    [Parameter(Mandatory = $true)][string]$AssetDirectory,
    [Parameter(Mandatory = $true)][string]$NotesPath,
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-fA-F]{40}$')][string]$TargetCommit
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/check-all-versions.ps1" -NoExit -Repo $Repo
$files = @(Get-BundleRequiredAssets | ForEach-Object {
    $file = Get-Item -LiteralPath (Join-Path $AssetDirectory $_)
    if ($file.PSIsContainer -or $file.Length -eq 0) { throw 'Invalid release asset' }
    $file.FullName
})
if (-not (Test-Path -LiteralPath $NotesPath -PathType Leaf)) { throw 'Missing release notes' }
$existing = gh release view $Tag --repo $Repo --json isDraft 2>$null
if ($LASTEXITCODE -ne 0) {
    gh release create $Tag --repo $Repo --target $TargetCommit --draft `
        --title 'AI Tools Portable Bundle' --notes-file $NotesPath
    if ($LASTEXITCODE -ne 0) { throw 'Release draft creation failed' }
}
gh release upload $Tag --repo $Repo @files --clobber
if ($LASTEXITCODE -ne 0) { throw 'Release asset upload failed' }
# gh release view resolves drafts; the REST tag endpoint can return 404 before publication.
$identityJson = gh release view $Tag --repo $Repo --json 'databaseId,tagName'
if ($LASTEXITCODE -ne 0) { throw 'Release identity lookup failed' }
$identity = $identityJson | ConvertFrom-Json
if ($identity.tagName -cne $Tag -or [string]$identity.databaseId -notmatch '^[1-9][0-9]*$') {
    throw 'Invalid release identity'
}
$remoteJson = gh api "repos/$Repo/releases/$($identity.databaseId)"
if ($LASTEXITCODE -ne 0) { throw 'Release asset verification failed' }
$remote = $remoteJson | ConvertFrom-Json
foreach ($path in $files) {
    $file = Get-Item -LiteralPath $path
    $matches = @($remote.assets | Where-Object { $_.name -ceq $file.Name })
    $digest = 'sha256:' + (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($matches.Count -ne 1 -or $matches[0].size -ne $file.Length -or $matches[0].digest -cne $digest) {
        throw 'Uploaded release asset does not match tested local bytes'
    }
}
gh release edit $Tag --repo $Repo --draft=false --latest `
    --title 'AI Tools Portable Bundle' --notes-file $NotesPath
if ($LASTEXITCODE -ne 0) { throw 'Release publication failed' }
Write-Output "Verified bundle published: $Tag"
