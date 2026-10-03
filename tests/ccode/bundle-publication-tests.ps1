$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../../scripts/check-all-versions.ps1" -NoExit
$root = Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString())
New-Item -ItemType Directory $root | Out-Null
try {
    foreach ($name in Get-BundleRequiredAssets) { Set-Content (Join-Path $root $name) 'fixture' }
    $notes = Join-Path $root 'notes.md'; Set-Content $notes 'fixture notes'
    function gh {
        $global:bundleTest_calls += ,@($args)
        $global:LASTEXITCODE = 0
        if ($args[0] -eq 'release' -and $args[1] -eq 'view') {
            if (-not $global:bundleTest_exists) { $global:LASTEXITCODE = 1; return }
            return '{"isDraft":false}'
        }
        if ($args[0] -eq 'release' -and $args[1] -eq 'upload' -and $global:bundleTest_failUpload) {
            $global:LASTEXITCODE = 1; return
        }
        if ($args[0] -eq 'api') {
            $assets = @(Get-BundleRequiredAssets | ForEach-Object {
                $file = Get-Item (Join-Path $root $_)
                @{ name=$_; size=$file.Length; digest=('sha256:' + (Get-FileHash $file.FullName).Hash.ToLowerInvariant()) }
            })
            if ($global:bundleTest_badDigest) { $assets[0].digest = 'sha256:wrong' }
            return (@{assets=$assets} | ConvertTo-Json -Depth 5)
        }
    }
    foreach ($scenario in @('new', 'existing', 'upload-failed', 'digest-mismatch')) {
        $global:bundleTest_exists = $scenario -ne 'new'
        $global:bundleTest_failUpload = $scenario -eq 'upload-failed'
        $global:bundleTest_badDigest = $scenario -eq 'digest-mismatch'
        $global:bundleTest_calls = @(); $failed = $false
        try {
            & "$PSScriptRoot/../../scripts/publish-bundle.ps1" -Tag 'fixture-tag' -Repo 'fixture/repo' `
                -AssetDirectory $root -NotesPath $notes -TargetCommit ('a' * 40)
        } catch { $failed = $true }
        $expectFailure = $scenario -in @('upload-failed', 'digest-mismatch')
        if ($failed -ne $expectFailure) { throw "Unexpected publication outcome: $scenario" }
        $commands = @($global:bundleTest_calls | ForEach-Object { $_ -join ' ' })
        if ($commands -match '^release delete ') { throw 'Must preserve existing release and tag' }
        $edits = @($commands | Where-Object { $_ -match '^release edit ' })
        if ($expectFailure -and $edits.Count) { throw 'Failed upload/digest must not publish' }
        if (-not $expectFailure -and $edits.Count -ne 1) { throw 'Verified bundle must publish once' }
        if ($scenario -eq 'new' -and -not ($commands -match '^release create .*--draft')) { throw 'New release must start as draft' }
        if ($global:bundleTest_exists -and ($commands -match '^release create ')) { throw 'Repair must reuse release' }
    }
    Write-Output 'PASS: bundle publication'
} finally { Remove-Item $root -Recurse -Force }
