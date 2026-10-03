param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedEngineVersion,

    [Parameter(Mandatory = $true)]
    [string]$AdapterRevision,

    [Parameter(Mandatory = $true)]
    [string]$EvidencePath,

    [ValidateSet('windows11-ordinary', 'github-hosted')]
    [string]$AcceptanceTarget = 'windows11-ordinary'
)

$ErrorActionPreference = "Stop"

if (-not $IsWindows) {
    throw "E_ACCEPTANCE_PLATFORM: Windows x64 is required"
}

$osArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture
$processArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture
if ($osArchitecture -ne [System.Runtime.InteropServices.Architecture]::X64 -or
    $processArchitecture -ne [System.Runtime.InteropServices.Architecture]::X64) {
    throw "E_ACCEPTANCE_PLATFORM: x64 OS and x64 process are required"
}

$currentVersion = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
$productName = [string]$currentVersion.ProductName
$installationType = [string]$currentVersion.InstallationType
$displayVersion = [string]$currentVersion.DisplayVersion
$build = 0
if (-not [int]::TryParse([string]$currentVersion.CurrentBuildNumber, [ref]$build)) {
    throw "E_ACCEPTANCE_PLATFORM: unreadable Windows build"
}
if ($AcceptanceTarget -eq 'windows11-ordinary' -and
    ($productName -notmatch 'Windows 11' -or $installationType -ne 'Client' -or $build -lt 22000)) {
    throw "E_ACCEPTANCE_PLATFORM: Windows 11 client build 22000 or newer is required"
}

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
$isAdministrator = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$administratorsSid = [Security.Principal.SecurityIdentifier]::new('S-1-5-32-544')
$administratorsMember = @($identity.Groups | ForEach-Object { $_.Value }) -contains $administratorsSid.Value
if ($AcceptanceTarget -eq 'windows11-ordinary' -and ($isAdministrator -or $administratorsMember)) {
    throw "E_ACCEPTANCE_ACCOUNT: an account outside the local Administrators group is required"
}

$source = (Resolve-Path $Executable).Path
$versionOutput = (& $source --version 2>&1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $versionOutput -notmatch "engine $([regex]::Escape($ExpectedEngineVersion))\)") {
    throw "E_ACCEPTANCE_ENGINE: expected engine $ExpectedEngineVersion"
}

$evidence = [ordered]@{
    schemaVersion = 1
    acceptanceTarget = $AcceptanceTarget
    collectedAtUtc = [DateTime]::UtcNow.ToString('o')
    platform = [ordered]@{
        productName = $productName
        displayVersion = $displayVersion
        build = $build
        installationType = $installationType
        osArchitecture = $osArchitecture.ToString()
        processArchitecture = $processArchitecture.ToString()
    }
    account = [ordered]@{
        administratorToken = $isAdministrator
        administratorsMember = $administratorsMember
    }
    package = [ordered]@{
        executableSha256 = (Get-FileHash -Path $source -Algorithm SHA256).Hash.ToLowerInvariant()
        engineVersion = $ExpectedEngineVersion
        versionOutput = $versionOutput
        adapterRevision = $AdapterRevision
    }
}

$destination = [System.IO.Path]::GetFullPath($EvidencePath)
$parent = Split-Path -Parent $destination
if ($parent) {
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
}
$evidence | ConvertTo-Json -Depth 5 | Set-Content -Path $destination -Encoding utf8NoBOM
Write-Output "Windows x64 acceptance environment verified for $AcceptanceTarget."
