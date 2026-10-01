param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = "Stop"

$source = (Resolve-Path $Executable).Path
$testRoot = Join-Path $env:RUNNER_TEMP "ccode test-$([Guid]::NewGuid().ToString('N'))"
$testExe = Join-Path $testRoot "ccode.exe"
$locationPushed = $false

function Remove-TestRoot {
    for ($attempt = 1; $attempt -le 20; $attempt++) {
        try {
            Remove-Item $testRoot -Recurse -Force -ErrorAction Stop
            return
        }
        catch {
            if ($attempt -eq 20) {
                throw
            }
            Start-Sleep -Milliseconds 500
        }
    }
}

function Invoke-ExpectExit {
    param(
        [string[]]$Arguments,
        [int]$Expected
    )

    & $testExe @Arguments
    if ($LASTEXITCODE -ne $Expected) {
        throw "Expected exit code $Expected, got $LASTEXITCODE for: $($Arguments -join ' ')"
    }
}

try {
    New-Item -ItemType Directory -Path $testRoot | Out-Null
    Copy-Item $source $testExe
    Push-Location $testRoot
    $locationPushed = $true

    Remove-Item Env:A_API_KEY -ErrorAction SilentlyContinue
    Remove-Item Env:A_AUTH_TOKEN -ErrorAction SilentlyContinue
    Remove-Item Env:A_BASE_URL -ErrorAction SilentlyContinue
    Invoke-ExpectExit -Arguments @() -Expected 64

    $env:A_AUTH_TOKEN = "test-only-token"
    Remove-Item Env:A_BASE_URL -ErrorAction SilentlyContinue
    Invoke-ExpectExit -Arguments @("--version") -Expected 0

    $diagnosticPath = Join-Path $testRoot 'failure-diagnostic.json'
    $env:A_AUTH_TOKEN = 'diagnostic-super-secret'
    Remove-Item Env:A_BASE_URL -ErrorAction SilentlyContinue
    Invoke-ExpectExit -Arguments @('--diagnostics', $diagnosticPath, '--print', 'sensitive diagnostic prompt') -Expected 64
    $diagnostic = Get-Content -LiteralPath $diagnosticPath -Raw | ConvertFrom-Json
    $operationId = [Guid]::Empty
    $validOperationId = [Guid]::TryParse([string]$diagnostic.operationId, [ref]$operationId)
    if ($diagnostic.schemaVersion -ne 1 -or $diagnostic.status -ne 'error' -or
        $diagnostic.errorCode -ne 'E_GATEWAY' -or $diagnostic.category -ne 'network' -or
        $diagnostic.exitCode -ne 64 -or -not $validOperationId -or
        $diagnostic.privacy.argumentsCaptured -or $diagnostic.privacy.environmentValuesCaptured -or
        $diagnostic.privacy.promptOrContentCaptured -or $diagnostic.privacy.credentialsCaptured) {
        throw 'Failure diagnostic schema or privacy flags are invalid'
    }
    $diagnosticText = Get-Content -LiteralPath $diagnosticPath -Raw
    foreach ($forbidden in @('diagnostic-super-secret', 'sensitive diagnostic prompt', $testRoot)) {
        if ($diagnosticText.Contains($forbidden, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Failure diagnostic retained a credential, prompt, or private path'
        }
    }
    $sentinelDiagnostic = Join-Path $testRoot 'diagnostic-sentinel.json'
    Set-Content -LiteralPath $sentinelDiagnostic -Value 'diagnostic-sentinel' -NoNewline
    $diagnosticStderr = Join-Path $testRoot 'diagnostic-stderr.txt'
    & $testExe --diagnostics $sentinelDiagnostic --print 'sensitive diagnostic prompt' 2> $diagnosticStderr
    if ($LASTEXITCODE -ne 64 -or (Get-Content -LiteralPath $sentinelDiagnostic -Raw) -ne 'diagnostic-sentinel' -or
        -not (Select-String -LiteralPath $diagnosticStderr -SimpleMatch 'E_DIAGNOSTIC_WRITE' -Quiet)) {
        throw 'Failure diagnostic overwrote an existing file or masked the primary exit code'
    }

    $env:A_BASE_URL = "http://gateway.example.test"
    Invoke-ExpectExit -Arguments @("login") -Expected 64
    Invoke-ExpectExit -Arguments @("--ccode-self-test") -Expected 0

    # Old releases rewrote .claude to .cc; an upgrade must recover transcripts.
    $homePath = Join-Path $testRoot 'data/cc/profile/home'
    $legacyProject = Join-Path $homePath '.cc/projects/D--tt'
    $currentProject = Join-Path $homePath '.claude/projects/D--tt'
    New-Item -ItemType Directory -Path $legacyProject -Force | Out-Null
    New-Item -ItemType Directory -Path $currentProject -Force | Out-Null
    Set-Content (Join-Path $legacyProject 'old-session.jsonl') 'legacy-session-marker'
    Set-Content (Join-Path $legacyProject 'existing-session.jsonl') 'old-copy'
    Set-Content (Join-Path $currentProject 'existing-session.jsonl') 'new-copy'
    $resumeFixture = Join-Path $PSScriptRoot '../../tests/ccode/resume-integration.py'
    python $resumeFixture prepare $testExe
    if ($LASTEXITCODE -ne 0) { throw 'Unable to prepare legacy resume fixture' }

    & $testExe --sessions
    if ($LASTEXITCODE -ne 0) {
        throw "The frontend failed its session discovery smoke test"
    }
    if (-not (Test-Path (Join-Path $currentProject 'old-session.jsonl'))) {
        throw 'Upgrade lost access to legacy .cc session transcripts'
    }
    if ((Get-Content (Join-Path $currentProject 'existing-session.jsonl') -Raw).Trim() -ne 'new-copy') {
        throw 'Upgrade overwrote an existing session'
    }
    if (-not (Test-Path (Join-Path $legacyProject 'old-session.jsonl'))) {
        throw 'Upgrade removed the original session backup'
    }
    python $resumeFixture verify $testExe
    if ($LASTEXITCODE -ne 0) { throw 'Official runtime could not resume recovered history' }

    $savedTemp = $env:TEMP
    try {
        $fixture = Join-Path $testRoot 'runtime-paths.js'
        Copy-Item (Join-Path $PSScriptRoot '../../tests/ccode/runtime-paths.js') $fixture
        Copy-Item (Join-Path $PSScriptRoot '../../tests/ccode/rename-contract.cjs') (Join-Path $testRoot 'rename-contract.cjs')
        bun test (Join-Path $PSScriptRoot '../../tests/ccode/rename-contract-tests.cjs')
        if ($LASTEXITCODE -ne 0) { throw 'Rename acceptance oracle regression failed' }
        $env:TEMP = Join-Path $testRoot 'data/cc/profile/temp'
        $payloadPath = (Get-ChildItem (Join-Path $testRoot 'runtime') -Filter engine.exe -Recurse | Select-Object -First 1).FullName
        python -c 'import subprocess,sys; sys.exit(subprocess.run(sys.argv[1:], timeout=60).returncode)' (Get-Command bun).Source $fixture $payloadPath
        if ($LASTEXITCODE -ne 0) { throw 'Bun runtime filesystem regression failed' }
    }
    finally {
        $env:TEMP = $savedTemp
    }
}
finally {
    if ($locationPushed) {
        Pop-Location
    }
    Remove-Item Env:A_API_KEY -ErrorAction SilentlyContinue
    Remove-Item Env:A_AUTH_TOKEN -ErrorAction SilentlyContinue
    Remove-Item Env:A_BASE_URL -ErrorAction SilentlyContinue
    if (Test-Path $testRoot) {
        Remove-TestRoot
    }
}
