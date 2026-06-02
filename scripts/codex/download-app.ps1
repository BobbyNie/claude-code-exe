# Download the full offline MSIX package for Codex desktop app from Microsoft Store.

param(
    [Parameter(Mandatory = $false)]
    [string]$Version = "store-latest",

    [Parameter(Mandatory = $false)]
    [string]$OutputDir = "."
)

$ErrorActionPreference = "Stop"

$ProductId = "9PLM9XGG6VKS"
$DownloadRoot = Join-Path $OutputDir "codex-app-msix-download"
$OutputFile = Join-Path $OutputDir "Codex.msix"

function Get-WingetPath {
    $command = Get-Command winget -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @()
    $windowsApps = Join-Path $env:ProgramFiles "WindowsApps"
    if (Test-Path $windowsApps) {
        $candidates += Get-ChildItem `
            -Path (Join-Path $windowsApps "Microsoft.DesktopAppInstaller_*_x64__8wekyb3d8bbwe\winget.exe") `
            -ErrorAction SilentlyContinue
    }

    $localWinget = Join-Path $env:LOCALAPPDATA "Microsoft\WindowsApps\winget.exe"
    if (Test-Path $localWinget) {
        $candidates += Get-Item $localWinget
    }

    $winget = $candidates | Sort-Object FullName -Descending | Select-Object -First 1
    if ($winget) {
        return $winget.FullName
    }

    return $null
}

Write-Output "=== Codex App Offline MSIX Download Script ==="
Write-Output "Target version: $Version"
Write-Output "Product ID: $ProductId"
Write-Output "Saving to: $OutputFile"

$wingetPath = Get-WingetPath
if (-not $wingetPath) {
    Write-Error "winget.exe is required to download the Microsoft Store offline MSIX package"
}

Write-Output "Using winget: $wingetPath"
Write-Output "Running winget download for Codex App"

if (Test-Path $DownloadRoot) {
    Remove-Item $DownloadRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $DownloadRoot | Out-Null

$wingetArgs = @(
    "download",
    "--id", $ProductId,
    "--exact",
    "--source", "msstore",
    "--skip-license",
    "--download-directory", $DownloadRoot,
    "--accept-source-agreements",
    "--accept-package-agreements",
    "--disable-interactivity"
)

& $wingetPath @wingetArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "winget download failed with exit code $LASTEXITCODE"
}

$packageFiles = Get-ChildItem -Path $DownloadRoot -Recurse -File |
    Where-Object { $_.Extension -in @(".msix", ".msixbundle", ".appx", ".appxbundle", ".msi") }

$codexPackage = $packageFiles |
    Where-Object { $_.Name -like "OpenAI.Codex*" -or $_.Name -like "*Codex*" } |
    Sort-Object Length -Descending |
    Select-Object -First 1

if (-not $codexPackage) {
    $codexPackage = $packageFiles | Sort-Object Length -Descending | Select-Object -First 1
}

if (-not $codexPackage) {
    Write-Error "winget download did not produce an offline app package"
}

if ($codexPackage.Extension -ine ".msix") {
    Write-Error "Expected Codex offline package to be MSIX, got: $($codexPackage.Name)"
}

Copy-Item $codexPackage.FullName $OutputFile -Force
Remove-Item $DownloadRoot -Recurse -Force

$fileInfo = Get-Item $OutputFile
Write-Output "Codex.msix size: $($fileInfo.Length) bytes ($([math]::Round($fileInfo.Length / 1MB, 2)) MB)"

if ($fileInfo.Length -lt 100MB) {
    Write-Error "Codex.msix is unexpectedly small; offline package may be incomplete"
}

Write-Output "Codex app offline MSIX download complete!"
