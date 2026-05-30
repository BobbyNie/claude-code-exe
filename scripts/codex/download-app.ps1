# Download the official Microsoft Store installer for Codex desktop app on Windows.

param(
    [Parameter(Mandatory = $false)]
    [string]$Version = "store-latest",

    [Parameter(Mandatory = $false)]
    [string]$OutputDir = "."
)

$ErrorActionPreference = "Stop"

$InstallerUrl = "https://get.microsoft.com/installer/download/9PLM9XGG6VKS?cid=github_release"
$OutputFile = Join-Path $OutputDir "Codex-Installer.exe"

Write-Output "=== Codex App Installer Download Script ==="
Write-Output "Target version: $Version"
Write-Output "Downloading from: $InstallerUrl"
Write-Output "Saving to: $OutputFile"

Invoke-WebRequest -Uri $InstallerUrl -OutFile $OutputFile -UseBasicParsing

$fileInfo = Get-Item $OutputFile
Write-Output "Codex installer size: $($fileInfo.Length) bytes ($([math]::Round($fileInfo.Length / 1MB, 2)) MB)"

if ($fileInfo.Length -lt 1MB) {
    Write-Error "Codex installer is unexpectedly small"
}

Write-Output "Codex app installer download complete!"
