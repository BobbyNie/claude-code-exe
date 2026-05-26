# Download the official Qwen Code Windows standalone archive.

param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $false)]
    [string]$OutputDir = "."
)

$ErrorActionPreference = "Stop"

$VersionTag = if ($Version.StartsWith("v")) { $Version } else { "v$Version" }
$BaseUrl = "https://qwen-code-assets.oss-cn-hangzhou.aliyuncs.com/releases/qwen-code/$VersionTag"
$ArchiveName = "qwen-code-win-x64.zip"
$ArchiveUrl = "$BaseUrl/$ArchiveName"
$ChecksumUrl = "$BaseUrl/SHA256SUMS"
$ArchivePath = Join-Path $OutputDir $ArchiveName
$ExtractDir = Join-Path $OutputDir "qwen-code"

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

function Get-ExpectedChecksum {
    param([string]$Url)

    try {
        $checksums = (Invoke-WebRequest -Uri $Url -UseBasicParsing).Content -split "`n"
        foreach ($line in $checksums) {
            $trimmed = $line.Trim()
            if ($trimmed -match "^([a-f0-9]{64})\s+$ArchiveName$") {
                return $Matches[1]
            }
        }
        Write-Warning "Checksum entry for $ArchiveName not found in SHA256SUMS"
        return $null
    }
    catch {
        Write-Warning "Could not fetch SHA256SUMS for verification"
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

    Write-Error "Checksum mismatch! Expected: $ExpectedHash, Got: $computedHash"
    return $false
}

function Expand-ArchiveSafely {
    param(
        [string]$ZipPath,
        [string]$Destination
    )

    if (Test-Path $Destination) {
        Remove-Item $Destination -Recurse -Force
    }

    Expand-Archive -Path $ZipPath -DestinationPath $OutputDir -Force

    if (-not (Test-Path (Join-Path $Destination "bin\qwen.cmd"))) {
        Write-Error "Extracted archive is missing bin\qwen.cmd"
        exit 1
    }
}

Write-Output "=== Qwen Code Portable Download Script ==="
Write-Output "Target version: $VersionTag"

$expectedChecksum = Get-ExpectedChecksum -Url $ChecksumUrl
Download-File -Url $ArchiveUrl -OutputPath $ArchivePath

if (-not (Verify-Checksum -FilePath $ArchivePath -ExpectedHash $expectedChecksum)) {
    Remove-Item $ArchivePath -Force
    exit 1
}

Expand-ArchiveSafely -ZipPath $ArchivePath -Destination $ExtractDir
Remove-Item $ArchivePath -Force

$rootSize = (Get-ChildItem $ExtractDir -Recurse | Measure-Object -Property Length -Sum).Sum
Write-Output "Extracted package size: $rootSize bytes ($([math]::Round($rootSize / 1MB, 2)) MB)"
Write-Output "Download and extraction complete!"
