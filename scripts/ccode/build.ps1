param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $false)]
    [string]$OutputDir = "."
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$baseUrl = "https://storage.googleapis.com/claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/claude-code-releases/$Version"
$work = Join-Path ([System.IO.Path]::GetTempPath()) "ccode-build-$([Guid]::NewGuid().ToString('N'))"
$payload = Join-Path $work "aa-runtime.bin"
$hook = Join-Path $work "cc-runtime.dll"
$manifestPath = Join-Path $work "manifest.json"
$resourceScript = Join-Path $work "ccode.rc"
$resourceObject = Join-Path $work "ccode.res"
$output = Join-Path (Resolve-Path $OutputDir) "ccode.exe"
$minHookVersion = "1.3.4"
$locationPushed = $false

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE"
    }
}

try {
    New-Item -ItemType Directory -Path $work | Out-Null
    Push-Location $work
    $locationPushed = $true

    Write-Output "Downloading and verifying official payload $Version..."
    Invoke-WebRequest -Uri "$baseUrl/manifest.json" -OutFile $manifestPath -UseBasicParsing
    Invoke-WebRequest -Uri "$baseUrl/win32-x64/claude.exe" -OutFile $payload -UseBasicParsing
    $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
    $expected = [string]$manifest.platforms.'win32-x64'.checksum
    $actual = (Get-FileHash -Path $payload -Algorithm SHA256).Hash.ToLowerInvariant()
    if (-not $expected -or $actual -ne $expected.ToLowerInvariant()) {
        throw "Official payload SHA256 mismatch. Expected $expected, got $actual"
    }

    Write-Output "Building the isolation layer with pinned MinHook $minHookVersion..."
    $minHookZip = Join-Path $work "minhook.zip"
    Invoke-WebRequest `
        -Uri "https://github.com/TsudaKageyu/minhook/archive/refs/tags/v$minHookVersion.zip" `
        -OutFile $minHookZip `
        -UseBasicParsing
    Expand-Archive -Path $minHookZip -DestinationPath $work
    $minHook = Join-Path $work "minhook-$minHookVersion"

    $hookSources = @(
        (Join-Path $PSScriptRoot "hook.cpp"),
        (Join-Path $minHook "src\buffer.c"),
        (Join-Path $minHook "src\hook.c"),
        (Join-Path $minHook "src\trampoline.c"),
        (Join-Path $minHook "src\hde\hde64.c")
    )
    $hookArgs = @(
        "/nologo", "/std:c++17", "/O2", "/EHsc", "/LD", "/DUNICODE", "/D_UNICODE",
        "/I$PSScriptRoot", "/I$(Join-Path $minHook 'include')", "/I$(Join-Path $minHook 'src')",
        "/Fe:$hook"
    ) + $hookSources + @("/link", "ws2_32.lib")
    Invoke-Checked cl.exe @hookArgs

    Write-Output "Embedding the verified payload and isolation layer as RCDATA resources..."
    $payloadRc = $payload.Replace('\', '\\')
    $hookRc = $hook.Replace('\', '\\')
    @"
101 RCDATA "$payloadRc"
102 RCDATA "$hookRc"
"@ | Set-Content -Path $resourceScript -Encoding ASCII
    Invoke-Checked rc.exe /nologo "/fo$resourceObject" $resourceScript

    $launcherArgs = @(
        "/nologo", "/std:c++17", "/O2", "/EHsc", "/DUNICODE", "/D_UNICODE",
        (Join-Path $PSScriptRoot "launcher.cpp"), $resourceObject,
        "/Fe:$output", "/link", "/SUBSYSTEM:CONSOLE"
    )
    Invoke-Checked cl.exe @launcherArgs

    Write-Output "Compiling and running native isolation tests..."
    $nativeTests = Join-Path $work "ccode-native-tests.exe"
    Invoke-Checked cl.exe /nologo /std:c++17 /O2 /EHsc `
        (Join-Path $PSScriptRoot "..\..\tests\ccode\native-tests.cpp") "/Fe:$nativeTests"
    Invoke-Checked $nativeTests

    Write-Output "Compiling and running hook integration tests..."
    $hookTests = Join-Path $work "ccode-hook-tests.exe"
    Invoke-Checked cl.exe /nologo /std:c++17 /O2 /EHsc `
        (Join-Path $PSScriptRoot "..\..\tests\ccode\hook-integration-tests.cpp") `
        "/Fe:$hookTests"
    Invoke-Checked $hookTests $hook

    $info = Get-Item $output
    if ($info.Length -le (Get-Item $payload).Length) {
        throw "ccode.exe is unexpectedly small; embedded resources may be missing"
    }
    Write-Output "Built single-file ccode.exe ($($info.Length) bytes)."
}
finally {
    if ($locationPushed) {
        Pop-Location
    }
    if (Test-Path $work) {
        Remove-Item $work -Recurse -Force
    }
}
