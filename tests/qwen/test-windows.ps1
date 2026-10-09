param([string]$LauncherSource = "$PSScriptRoot/../../scripts/qwen/launcher.cs")
$ErrorActionPreference = 'Stop'
$LauncherSource = (Resolve-Path -LiteralPath $LauncherSource).Path
$testSource = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'launcher-tests.cs')).Path
$compiler = "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
$directory = Join-Path $env:RUNNER_TEMP ('qwen launcher tests ' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $directory | Out-Null
$exe = Join-Path $directory 'tests.exe'
& $compiler /nologo /target:exe /main:LauncherTests "/out:$exe" `
    /reference:System.IO.Compression.dll /reference:System.IO.Compression.FileSystem.dll `
    $LauncherSource $testSource
if ($LASTEXITCODE -ne 0) { throw 'Launcher test compilation failed' }
# cmd provides a real console on hosted runners, not redirected terminal pipes.
& cmd.exe /d /c "`"$exe`""
if ($LASTEXITCODE -ne 0) { throw 'Qwen Windows launcher tests failed' }
