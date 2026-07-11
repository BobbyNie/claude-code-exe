param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = "Stop"

$source = (Resolve-Path $Executable).Path
$testRoot = Join-Path $env:RUNNER_TEMP "ccode-test-$([Guid]::NewGuid().ToString('N'))"
$testExe = Join-Path $testRoot "ccode.exe"
$locationPushed = $false

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
    Remove-Item Env:A_BASE_URL -ErrorAction SilentlyContinue
    Invoke-ExpectExit -Arguments @() -Expected 64

    $env:A_API_KEY = "test-only-key"
    $env:A_BASE_URL = "http://gateway.example.test"
    Invoke-ExpectExit -Arguments @("--version") -Expected 64

    $env:A_BASE_URL = "https://gateway.example.test"
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
}
finally {
    if ($locationPushed) {
        Pop-Location
    }
    Remove-Item Env:A_API_KEY -ErrorAction SilentlyContinue
    Remove-Item Env:A_BASE_URL -ErrorAction SilentlyContinue
    if (Test-Path $testRoot) {
        Remove-Item $testRoot -Recurse -Force
    }
}
