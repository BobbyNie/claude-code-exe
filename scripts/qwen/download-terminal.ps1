# Bundle Microsoft's unpackaged x64 distribution. Do not use the system wt.exe alias.
param([string]$OutputDir = '.')
$ErrorActionPreference = 'Stop'
$pin = Get-Content (Join-Path $PSScriptRoot 'terminal-release.json') -Raw | ConvertFrom-Json
if ($pin.version -notmatch '^\d+(\.\d+){3}$' -or $pin.asset -ne "Microsoft.WindowsTerminal_$($pin.version)_x64.zip" -or $pin.sha256 -notmatch '^[0-9a-f]{64}$') {
    throw 'Invalid Windows Terminal release pin'
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$stage = Join-Path $OutputDir ('terminal-build-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    $rawZip = Join-Path $stage $pin.asset
    $url = "https://github.com/microsoft/terminal/releases/download/v$($pin.version)/$($pin.asset)"
    Write-Output "Downloading pinned Windows Terminal $($pin.version)"
    Invoke-WebRequest -Uri $url -OutFile $rawZip -UseBasicParsing
    if ((Get-FileHash $rawZip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.sha256) {
        throw "Windows Terminal SHA256 mismatch"
    }
    $expanded = Join-Path $stage 'expanded'
    Expand-Archive -LiteralPath $rawZip -DestinationPath $expanded
    $source = Join-Path $expanded "terminal-$($pin.version)"
    foreach ($required in @('WindowsTerminal.exe', 'Microsoft.Terminal.Settings.Model.dll', 'NOTICE.html', 'OpenConsole.exe', 'Microsoft.UI.Xaml.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $source $required))) { throw "Terminal archive missing $required" }
    }
    Copy-Item (Join-Path $PSScriptRoot 'terminal-LICENSE') (Join-Path $source 'LICENSE')
    # ZipFile includes dot-files and hidden files on every build host.
    [IO.File]::WriteAllText((Join-Path $source '.portable'), '')
    $settingsDir = Join-Path $source 'settings'
    New-Item -ItemType Directory -Path $settingsDir -Force | Out-Null
    @'
{
  "defaultProfile": "{4e28b223-8a1c-4afb-8d70-a0b8de748e2f}",
  "firstWindowPreference": "defaultProfile",
  "profiles": {
    "list": [
      {
        "guid": "{4e28b223-8a1c-4afb-8d70-a0b8de748e2f}",
        "name": "Qwen",
        "commandline": "cmd.exe",
        "closeOnExit": "always"
      }
    ]
  }
}
'@ | Set-Content (Join-Path $settingsDir 'settings.json') -Encoding UTF8
    $zip = [IO.Path]::GetFullPath((Join-Path $OutputDir 'qwen-terminal.zip'))
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory([IO.Path]::GetFullPath($source), $zip)
    Set-Content (Join-Path $OutputDir 'qwen-terminal-version.txt') $pin.version -Encoding ASCII -NoNewline
    Write-Output 'Terminal archive verified and prepared with portable settings and license notices'
}
finally { Remove-Item $stage -Recurse -Force }
