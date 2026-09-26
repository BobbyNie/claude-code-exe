param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedEngineVersion,

    [Parameter(Mandatory = $true)]
    [string]$AdapterRevision,

    [Parameter(Mandatory = $true)]
    [string]$EvidencePath,

    [string]$PythonCommand = 'python',

    [string]$WorkingRoot,

    [switch]$KeepWorkingDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-StringSha256 {
    param([Parameter(Mandatory = $true)][string]$Value)

    $bytes = [Text.Encoding]::UTF8.GetBytes($Value)
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([Convert]::ToHexString($sha.ComputeHash($bytes))).ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Get-DirectoryManifest {
    param([Parameter(Mandatory = $true)][string]$Root)

    if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
        return @()
    }
    $absoluteRoot = [IO.Path]::GetFullPath($Root)
    return @(
        Get-ChildItem -LiteralPath $absoluteRoot -File -Force -Recurse |
            Sort-Object FullName |
            ForEach-Object {
                [ordered]@{
                    path = ([IO.Path]::GetRelativePath($absoluteRoot, $_.FullName) -replace '\\', '/')
                    size = $_.Length
                    sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                }
            }
    )
}

function Get-ServiceInventory {
    return @(
        Get-Service | Sort-Object Name | ForEach-Object {
            [ordered]@{
                name = $_.Name
                status = $_.Status.ToString()
                startType = $_.StartType.ToString()
            }
        }
    )
}

function Get-DriverInventory {
    return @(
        Get-CimInstance Win32_SystemDriver | Sort-Object Name | ForEach-Object {
            [ordered]@{
                name = [string]$_.Name
                state = [string]$_.State
                startMode = [string]$_.StartMode
                serviceType = [string]$_.ServiceType
            }
        }
    )
}

function Get-InventoryDigest {
    param([Parameter(Mandatory = $true)][object[]]$Inventory)
    return Get-StringSha256 ($Inventory | ConvertTo-Json -Depth 5 -Compress)
}

function Compare-NamedInventory {
    param(
        [Parameter(Mandatory = $true)][object[]]$Before,
        [Parameter(Mandatory = $true)][object[]]$After
    )

    $beforeByName = @{}
    $afterByName = @{}
    foreach ($item in $Before) { $beforeByName[[string]$item.name] = $item }
    foreach ($item in $After) { $afterByName[[string]$item.name] = $item }

    $added = @($afterByName.Keys | Where-Object { -not $beforeByName.ContainsKey($_) } | Sort-Object)
    $removed = @($beforeByName.Keys | Where-Object { -not $afterByName.ContainsKey($_) } | Sort-Object)
    $changed = @(
        $beforeByName.Keys | Where-Object { $afterByName.ContainsKey($_) } | Sort-Object |
            Where-Object {
                ($beforeByName[$_] | ConvertTo-Json -Depth 5 -Compress) -ne
                    ($afterByName[$_] | ConvertTo-Json -Depth 5 -Compress)
            }
    )
    return [ordered]@{ added = $added; removed = $removed; changed = $changed }
}

function Invoke-RecordedProcess {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [string[]]$Arguments = @(),
        [Parameter(Mandatory = $true)][string]$CurrentDirectory,
        [int]$TimeoutSeconds = 900
    )

    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $FilePath
    $start.WorkingDirectory = $CurrentDirectory
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $Arguments) {
        [void]$start.ArgumentList.Add($argument)
    }

    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    if (-not $process.Start()) {
        throw "E_ACCEPTANCE_PROCESS: unable to start $FilePath"
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill($true)
        $process.WaitForExit()
        throw "E_ACCEPTANCE_TIMEOUT: $FilePath"
    }
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    return [ordered]@{
        file = [IO.Path]::GetFileName($FilePath)
        arguments = @($Arguments)
        exitCode = $process.ExitCode
        stdout = $stdout.Trim()
        stderr = $stderr.Trim()
    }
}

function Assert-Success {
    param(
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Result,
        [Parameter(Mandatory = $true)][string]$Label
    )
    if ($Result.exitCode -ne 0) {
        throw "E_ACCEPTANCE_COMMAND: $Label exited $($Result.exitCode): $($Result.stderr)"
    }
}

function ConvertTo-SafeText {
    param([AllowNull()][string]$Value)

    if ($null -eq $Value) { return $null }
    $safe = $Value
    $replacements = [ordered]@{
        $acceptanceRoot = '<acceptance-root>'
        $repositoryRoot = '<repository-root>'
        $source = '<source-executable>'
        ([Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile)) = '<user-profile>'
    }
    foreach ($entry in $replacements.GetEnumerator()) {
        if ($entry.Key) {
            $safe = $safe.Replace([string]$entry.Key, [string]$entry.Value, [StringComparison]::OrdinalIgnoreCase)
        }
    }
    return $safe
}

function ConvertTo-SafeCommandEvidence {
    param([Parameter(Mandatory = $true)][System.Collections.IDictionary]$Result)

    return [ordered]@{
        file = $Result.file
        arguments = @($Result.arguments | ForEach-Object { ConvertTo-SafeText ([string]$_) })
        exitCode = $Result.exitCode
        stdout = ConvertTo-SafeText ([string]$Result.stdout)
        stderr = ConvertTo-SafeText ([string]$Result.stderr)
    }
}

$source = (Resolve-Path -LiteralPath $Executable).Path
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$platformGate = Join-Path $PSScriptRoot 'assert-windows11-x64.ps1'
$toolsFixture = Join-Path $repositoryRoot 'tests/ccode/tools-integration.py'
if (-not (Test-Path -LiteralPath $platformGate -PathType Leaf) -or
    -not (Test-Path -LiteralPath $toolsFixture -PathType Leaf)) {
    throw 'E_ACCEPTANCE_HARNESS: repository acceptance files are incomplete'
}

if ($WorkingRoot) {
    $acceptanceRoot = [IO.Path]::GetFullPath($WorkingRoot)
    if (Test-Path -LiteralPath $acceptanceRoot) {
        if (@(Get-ChildItem -LiteralPath $acceptanceRoot -Force).Count -ne 0) {
            throw 'E_ACCEPTANCE_ROOT: working root must be absent or empty'
        }
    }
}
else {
    $acceptanceRoot = Join-Path ([IO.Path]::GetTempPath()) "ccode 離線驗收 $([Guid]::NewGuid().ToString('N'))"
}
$evidenceDestination = [IO.Path]::GetFullPath($EvidencePath)
$acceptancePrefix = $acceptanceRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if ($evidenceDestination.StartsWith($acceptancePrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'E_EVIDENCE_PATH: evidence must be outside the disposable working root'
}

$programRoot = Join-Path $acceptanceRoot 'portable app 中文'
$workspaceRoot = Join-Path $acceptanceRoot 'workspace 中文 with spaces'
$dataRoot = Join-Path $acceptanceRoot 'persistent data 中文'
$platformEvidencePath = Join-Path $acceptanceRoot 'platform-evidence.json'
New-Item -ItemType Directory -Path $programRoot, $workspaceRoot, $dataRoot -Force | Out-Null
$app = Join-Path $programRoot 'ccode.exe'
Copy-Item -LiteralPath $source -Destination $app

# The ordinary-account, Windows 11 Client, OS x64 and process x64 gate is shared
# with the connected test entry point, but this verifier itself is standalone.
& $platformGate -Executable $app -ExpectedEngineVersion $ExpectedEngineVersion `
    -AdapterRevision $AdapterRevision -EvidencePath $platformEvidencePath | Out-Null
$platformEvidence = Get-Content -LiteralPath $platformEvidencePath -Raw | ConvertFrom-Json

$routeInventory = @(
    Get-NetRoute -ErrorAction Stop |
        Where-Object { $_.DestinationPrefix -in @('0.0.0.0/0', '::/0') } |
        Sort-Object AddressFamily, InterfaceIndex, RouteMetric |
        ForEach-Object {
            $alias = [string]$_.InterfaceAlias
            [ordered]@{
                addressFamily = $_.AddressFamily.ToString()
                destinationPrefix = [string]$_.DestinationPrefix
                interfaceIndex = $_.InterfaceIndex
                interfaceAliasSha256 = Get-StringSha256 $alias
                loopbackInterface = ($_.InterfaceIndex -eq 1 -or $alias -match '(?i)loopback')
                routeMetric = $_.RouteMetric
                nextHopIsLoopback = ([string]$_.NextHop -in @('127.0.0.1', '::1'))
            }
        }
)
$adapterInventory = @(
    Get-NetAdapter -IncludeHidden -ErrorAction Stop | Sort-Object ifIndex | ForEach-Object {
        $name = [string]$_.Name
        $description = [string]$_.InterfaceDescription
        [ordered]@{
            interfaceIndex = $_.ifIndex
            nameSha256 = Get-StringSha256 $name
            descriptionSha256 = Get-StringSha256 $description
            loopbackInterface = ($_.ifIndex -eq 1 -or $name -match '(?i)loopback' -or
                $description -match '(?i)loopback')
            status = $_.Status.ToString()
            hardwareInterface = [bool]$_.HardwareInterface
        }
    }
)
$nonLoopbackDefaultRoutes = @($routeInventory | Where-Object { -not $_.loopbackInterface })
$connectedNonLoopbackAdapters = @($adapterInventory | Where-Object {
    $_.status -eq 'Up' -and -not $_.loopbackInterface
})
if ($nonLoopbackDefaultRoutes.Count -ne 0 -or $connectedNonLoopbackAdapters.Count -ne 0) {
    throw 'E_OFFLINE_ROUTE: disable or physically disconnect every non-loopback network adapter before acceptance'
}

$serviceBefore = Get-ServiceInventory
$driverBefore = Get-DriverInventory
$programBefore = Get-DirectoryManifest $programRoot
$commands = [Collections.Generic.List[object]]::new()
$failure = $null
$packageManifest = $null
$workspaceIdentity = $null

try {
    $version = Invoke-RecordedProcess -FilePath $app -Arguments @('--version') -CurrentDirectory $workspaceRoot
    [void]$commands.Add($version)
    Assert-Success $version 'version'
    if ($version.stdout -notmatch "engine $([regex]::Escape($ExpectedEngineVersion))\)") {
        throw "E_ACCEPTANCE_ENGINE: expected engine $ExpectedEngineVersion"
    }

    $selfTest = Invoke-RecordedProcess -FilePath $app -Arguments @('--ccode-self-test') -CurrentDirectory $workspaceRoot
    [void]$commands.Add($selfTest)
    Assert-Success $selfTest 'self-test'

    $manifestResult = Invoke-RecordedProcess -FilePath $app -Arguments @('--package-manifest') -CurrentDirectory $workspaceRoot
    [void]$commands.Add($manifestResult)
    Assert-Success $manifestResult 'package manifest'
    $packageManifest = $manifestResult.stdout | ConvertFrom-Json
    if ($packageManifest.engineVersion -ne $ExpectedEngineVersion -or
        $packageManifest.adapterRevision -ne $AdapterRevision) {
        throw 'E_PACKAGE_METADATA: package identity does not match the acceptance request'
    }

    $identityResult = Invoke-RecordedProcess -FilePath $app `
        -Arguments @('--data-dir', $dataRoot, '--workspace-id') -CurrentDirectory $workspaceRoot
    [void]$commands.Add($identityResult)
    Assert-Success $identityResult 'workspace identity'
    $workspaceIdentity = $identityResult.stdout

    # tools-integration.py starts a loopbackFixture that returns deterministic model
    # responses. The engine, frontend, child processes and filesystem tools are real;
    # this is not a live model or enterprise gateway acceptance result.
    $toolsResult = Invoke-RecordedProcess -FilePath $PythonCommand `
        -Arguments @($toolsFixture, $app, '--acceptance-root', $acceptanceRoot) `
        -CurrentDirectory $repositoryRoot
    [void]$commands.Add($toolsResult)
    Assert-Success $toolsResult 'real engine and tools through loopback fixture'
}
catch {
    $failure = $_.Exception.Message
}

$programAfter = Get-DirectoryManifest $programRoot
$dataRootManifest = Get-DirectoryManifest $dataRoot
$workspaceManifest = Get-DirectoryManifest $workspaceRoot
$serviceAfter = Get-ServiceInventory
$driverAfter = Get-DriverInventory
$serviceDelta = Compare-NamedInventory -Before $serviceBefore -After $serviceAfter
$driverDelta = Compare-NamedInventory -Before $driverBefore -After $driverAfter

if ($serviceDelta.added.Count -ne 0 -and -not $failure) {
    $failure = "E_OFFLINE_SERVICE: acceptance created services: $($serviceDelta.added -join ', ')"
}
if ($driverDelta.added.Count -ne 0 -and -not $failure) {
    $failure = "E_OFFLINE_DRIVER: acceptance created drivers: $($driverDelta.added -join ', ')"
}

if ($packageManifest) {
    $allowedRuntimePaths = @(
        "runtime/$($packageManifest.engineSha256)/engine.exe",
        "runtime/$($packageManifest.engineSha256)/prepare.lock"
    )
    $beforeByPath = @{}
    foreach ($file in $programBefore) { $beforeByPath[$file.path] = $file }
    $unexpectedProgramFiles = @($programAfter | Where-Object {
        -not $beforeByPath.ContainsKey($_.path) -and $_.path -notin $allowedRuntimePaths
    } | ForEach-Object { $_.path })
    $changedProgramFiles = @($programAfter | Where-Object {
        $beforeByPath.ContainsKey($_.path) -and
            ($beforeByPath[$_.path].sha256 -ne $_.sha256 -or $beforeByPath[$_.path].size -ne $_.size)
    } | ForEach-Object { $_.path })
    $engineEntry = @($programAfter | Where-Object {
        $_.path -eq "runtime/$($packageManifest.engineSha256)/engine.exe"
    })
    if (($unexpectedProgramFiles.Count -ne 0 -or $changedProgramFiles.Count -ne 0 -or
         $engineEntry.Count -ne 1 -or $engineEntry[0].sha256 -ne $packageManifest.engineSha256) -and
        -not $failure) {
        $failure = 'E_PROGRAM_DATA_LEAK: program directory contains unexpected or modified runtime data'
    }
}

if ($dataRootManifest.Count -eq 0 -and -not $failure) {
    $failure = 'E_DATA_ROOT: real engine trial did not create external persistent data'
}

$evidence = [ordered]@{
    schemaVersion = 1
    collectedAtUtc = [DateTime]::UtcNow.ToString('o')
    acceptance = [ordered]@{
        scope = 'Windows 11 x64 offline ordinary-account package trial'
        passed = -not [bool]$failure
        failure = ConvertTo-SafeText $failure
        loopbackFixture = [ordered]@{
            used = $true
            limitation = 'deterministic local response fixture; not a live model or enterprise gateway'
        }
    }
    platform = $platformEvidence.platform
    account = $platformEvidence.account
    package = [ordered]@{
        executableSha256 = (Get-FileHash -LiteralPath $app -Algorithm SHA256).Hash.ToLowerInvariant()
        adapterRevision = $AdapterRevision
        engineVersion = $ExpectedEngineVersion
        manifest = $packageManifest
    }
    offline = [ordered]@{
        defaultRoutes = $routeInventory
        adapters = $adapterInventory
        nonLoopbackDefaultRouteCount = $nonLoopbackDefaultRoutes.Count
        connectedNonLoopbackAdapterCount = $connectedNonLoopbackAdapters.Count
        assertion = 'No non-loopback default route or connected non-loopback adapter was present during the trial.'
    }
    hostInventory = [ordered]@{
        services = [ordered]@{
            beforeSha256 = Get-InventoryDigest $serviceBefore
            afterSha256 = Get-InventoryDigest $serviceAfter
            delta = $serviceDelta
        }
        drivers = [ordered]@{
            beforeSha256 = Get-InventoryDigest $driverBefore
            afterSha256 = Get-InventoryDigest $driverAfter
            delta = $driverDelta
        }
    }
    filesystem = [ordered]@{
        programBefore = $programBefore
        programAfter = $programAfter
        dataRootManifest = $dataRootManifest
        workspaceManifest = $workspaceManifest
    }
    execution = [ordered]@{
        workspaceIdentity = $workspaceIdentity
        commands = @($commands | ForEach-Object { ConvertTo-SafeCommandEvidence $_ })
        harnessDependencies = @('PowerShell 7', 'Python 3')
        packageRuntimeDependencies = @('ccode.exe embedded payload', 'approved Windows system components')
    }
}

$evidenceParent = Split-Path -Parent $evidenceDestination
if ($evidenceParent) {
    New-Item -ItemType Directory -Path $evidenceParent -Force | Out-Null
}
$evidence | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $evidenceDestination -Encoding utf8NoBOM

if (-not $KeepWorkingDirectory) {
    Remove-Item -LiteralPath $acceptanceRoot -Recurse -Force
}
if ($failure) {
    throw $failure
}
Write-Output "Offline Windows 11 x64 acceptance passed. Evidence: $evidenceDestination"
