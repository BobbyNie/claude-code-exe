# Download Script
# Downloads the official Claude Windows executable and prepares it for portable packaging

param(
    [Parameter(Mandatory=$true)]
    [string]$Version,

    [Parameter(Mandatory=$false)]
    [string]$OutputDir = "."
)

$ErrorActionPreference = "Stop"

# Constants
$BaseUrl = "https://storage.googleapis.com/claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/claude-code-releases"
$Platform = "win32-x64"
$DownloadUrl = "$BaseUrl/$Version/$Platform/claude.exe"
$OutputFile = Join-Path $OutputDir "cloude.exe"

function Download-File {
    param(
        [string]$Url,
        [string]$OutputPath
    )

    Write-Output "Downloading from: $Url"
    Write-Output "Saving to: $OutputPath"

    try {
        Invoke-WebRequest -Uri $Url -OutFile $OutputPath -UseBasicParsing
        Write-Output "Download completed successfully"
    }
    catch {
        Write-Error "Download failed: $_"
        exit 1
    }
}

function Get-Manifest {
    param([string]$Version)

    $manifestUrl = "$BaseUrl/$Version/manifest.json"
    try {
        $manifest = Invoke-RestMethod -Uri $manifestUrl -Method Get
        return $manifest
    }
    catch {
        Write-Warning "Could not fetch manifest for verification"
        return $null
    }
}

function Verify-Checksum {
    param(
        [string]$FilePath,
        [string]$ExpectedHash
    )

    if (-not $ExpectedHash) {
        Write-Warning "No checksum available for verification"
        return $true
    }

    Write-Output "Verifying checksum..."

    $fileStream = [System.IO.File]::OpenRead($FilePath)
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    $hash = $sha256.ComputeHash($fileStream)
    $fileStream.Close()
    $computedHash = [System.BitConverter]::ToString($hash).Replace("-", "").ToLower()

    if ($computedHash -eq $ExpectedHash.ToLower()) {
        Write-Output "Checksum verification passed"
        return $true
    }
    else {
        Write-Error "Checksum mismatch! Expected: $ExpectedHash, Got: $computedHash"
        return $false
    }
}

# Main execution
Write-Output "=== Claude Code Portable Download Script ==="
Write-Output "Target version: $Version"

# Get manifest for checksum verification
$manifest = Get-Manifest -Version $Version
$expectedChecksum = $null
if ($manifest -and $manifest.files) {
    $platformFile = $manifest.files | Where-Object { $_.platform -eq $Platform }
    if ($platformFile) {
        $expectedChecksum = $platformFile.sha256
    }
}

# Download the file
Download-File -Url $DownloadUrl -OutputPath $OutputFile

# Verify checksum if available
if (-not (Verify-Checksum -FilePath $OutputFile -ExpectedHash $expectedChecksum)) {
    Remove-Item $OutputFile -Force
    exit 1
}

# Output file info
$fileInfo = Get-Item $OutputFile
Write-Output "Downloaded file size: $($fileInfo.Length) bytes ($([math]::Round($fileInfo.Length / 1MB, 2)) MB)"

Write-Output "Download and preparation complete!"
