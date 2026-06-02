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
$HelperUrl = "https://raw.githubusercontent.com/MattiasC85/Scripts/3687ec33533443bd47e09fada3ec180a78383b5b/OSD/GetStoreURL.ps1"
$HelperPath = Join-Path $OutputDir "GetStoreURL.ps1"

Write-Output "=== Codex App Offline MSIX Download Script ==="
Write-Output "Target version: $Version"
Write-Output "Product ID: $ProductId"
Write-Output "Saving to: $OutputFile"

if (Test-Path $DownloadRoot) {
    Remove-Item $DownloadRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $DownloadRoot | Out-Null

Write-Output "Downloading Microsoft Store URL helper: $HelperUrl"
Invoke-WebRequest -Uri $HelperUrl -OutFile $HelperPath -UseBasicParsing

. $HelperPath

if (-not (Get-Command Get-StoreURLs -ErrorAction SilentlyContinue)) {
    Write-Error "GetStoreURL.ps1 did not define Get-StoreURLs"
}

Write-Output "Resolving temporary Microsoft CDN URLs via fe3.delivery.mp.microsoft.com"
$storeUrls = @(Get-StoreURLs -ProductNumber $ProductId -Architecture x64)
$codexEntry = $storeUrls |
    Where-Object { $_.FileName -match "OpenAI\.Codex.*_x64.*\.msix$" } |
    Select-Object -First 1

if (-not $codexEntry) {
    Write-Output "Available Microsoft Store package entries:"
    $storeUrls | ForEach-Object { Write-Output "  - $($_.FileName)" }
    Write-Error "Could not find Codex x64 MSIX package in Microsoft Store response"
}

Write-Output "Selected package: $($codexEntry.FileName)"

$downloaded = $false
foreach ($url in @($codexEntry.URLS)) {
    try {
        Write-Output "Downloading from Microsoft CDN: $url"
        Invoke-WebRequest -Uri $url -OutFile $OutputFile -UseBasicParsing
        $downloaded = $true
        break
    }
    catch {
        Write-Warning "Download URL failed: $_"
    }
}

if (-not $downloaded) {
    Write-Error "All Microsoft CDN download URLs failed"
}

Remove-Item $DownloadRoot -Recurse -Force

$fileInfo = Get-Item $OutputFile
Write-Output "Codex.msix size: $($fileInfo.Length) bytes ($([math]::Round($fileInfo.Length / 1MB, 2)) MB)"

if ($fileInfo.Length -lt 100MB) {
    Write-Error "Codex.msix is unexpectedly small; offline package may be incomplete"
}

Write-Output "Codex app offline MSIX download complete!"
