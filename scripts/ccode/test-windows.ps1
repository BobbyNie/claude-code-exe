param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = "Stop"

$source = (Resolve-Path $Executable).Path
$testRoot = Join-Path $env:RUNNER_TEMP "ccode-test-$([Guid]::NewGuid().ToString('N'))"
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
    Invoke-ExpectExit -Arguments @("--version") -Expected 64

    $env:A_BASE_URL = "http://gateway.example.test"
    Invoke-ExpectExit -Arguments @("login") -Expected 64
    Invoke-ExpectExit -Arguments @("--ccode-self-test") -Expected 0

    & $testExe --version
    if ($LASTEXITCODE -ne 0) {
        throw "The embedded official payload failed its injected --version smoke test"
    }

    $savedTemp = $env:TEMP
    try {
        $fixture = Join-Path $testRoot 'runtime-paths.js'
        Copy-Item (Join-Path $PSScriptRoot '../../tests/ccode/runtime-paths.js') $fixture
        $env:TEMP = Join-Path $testRoot 'data/cc/profile/temp'
        $hookPath = Join-Path $testRoot 'data/cc/runtime/cc-runtime.dll'
        python -c 'import subprocess,sys; sys.exit(subprocess.run(sys.argv[1:], timeout=30).returncode)' (Get-Command bun).Source $fixture $hookPath
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
