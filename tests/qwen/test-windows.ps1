param(
    [string]$LauncherSource = "$PSScriptRoot/../../scripts/qwen/launcher.cs",
    [string]$PackagedExe = ''
)
$ErrorActionPreference = 'Stop'
$LauncherSource = (Resolve-Path -LiteralPath $LauncherSource).Path
$terminalSource = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../scripts/qwen/terminal-host.cs')).Path
$terminalTests = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'terminal-tests.cs')).Path
$testSource = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'launcher-tests.cs')).Path
$compiler = "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
$directory = Join-Path $env:RUNNER_TEMP ('qwen launcher tests ; 中文 ' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $directory | Out-Null
$exe = Join-Path $directory 'tests.exe'
& $compiler /nologo /codepage:65001 /target:exe /main:LauncherTests "/out:$exe" `
    /reference:System.IO.Compression.dll /reference:System.IO.Compression.FileSystem.dll `
    $LauncherSource $terminalSource $terminalTests $testSource
if ($LASTEXITCODE -ne 0) { throw 'Launcher test compilation failed' }
# The native test allocates a console if needed and verifies shared console ownership.
& cmd.exe /d /c "`"$exe`""
if ($LASTEXITCODE -ne 0) { throw 'Qwen Windows launcher tests failed' }

if ($PackagedExe) {
    $PackagedExe = (Resolve-Path -LiteralPath $PackagedExe).Path
    & $exe --terminal-integration $PackagedExe
    if ($LASTEXITCODE -ne 0) { throw 'Bundled Windows Terminal integration failed' }
}
