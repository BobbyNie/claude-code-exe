# Download the official Codex CLI Windows x64 executable from openai/codex.

param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $false)]
    [string]$OutputDir = "."
)

$ErrorActionPreference = "Stop"

$ReleaseTag = if ($Version.StartsWith("rust-v")) { $Version } else { "rust-v$Version" }
$AssetName = "codex-x86_64-pc-windows-msvc.exe"
$DownloadUrl = "https://github.com/openai/codex/releases/download/$ReleaseTag/$AssetName"
$OutputFile = Join-Path $OutputDir "codex.exe"

Write-Output "=== Codex CLI Download Script ==="
Write-Output "Target version: $Version"
Write-Output "Release tag: $ReleaseTag"
Write-Output "Downloading from: $DownloadUrl"
Write-Output "Saving to: $OutputFile"

Invoke-WebRequest -Uri $DownloadUrl -OutFile $OutputFile -UseBasicParsing

$fileInfo = Get-Item $OutputFile
Write-Output "codex.exe size: $($fileInfo.Length) bytes ($([math]::Round($fileInfo.Length / 1MB, 2)) MB)"

if ($fileInfo.Length -lt 10MB) {
    Write-Error "codex.exe is unexpectedly small; the standalone CLI binary may be missing"
}

Write-Output "Codex CLI download complete!"
