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

    $forbiddenNames = Get-ChildItem $testRoot -Recurse -Force | Where-Object {
        $_.Name -match '(?i)anthropic|claude'
    }
    if ($forbiddenNames) {
        throw "Forbidden filesystem names were created: $($forbiddenNames.FullName -join ', ')"
    }

    $env:BUN_BE_BUN = '1'
    try {
        $fixture = Join-Path $testRoot 'runtime-paths.js'
        Copy-Item (Join-Path $PSScriptRoot '../../tests/ccode/runtime-paths.js') $fixture
        Invoke-ExpectExit -Arguments @($fixture) -Expected 0
    }
    finally {
        Remove-Item Env:BUN_BE_BUN -ErrorAction SilentlyContinue
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
