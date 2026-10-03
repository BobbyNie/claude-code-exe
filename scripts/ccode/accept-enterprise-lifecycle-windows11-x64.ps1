param(
    [Parameter(Mandatory = $true)]
    [string]$CandidateRoot,

    [Parameter(Mandatory = $true)]
    [string]$EvidencePath,

    [Parameter(Mandatory = $true)]
    [string]$SignaturePath,

    [Parameter(Mandatory = $true)]
    [string]$PublicKeyPath,

    [Parameter(Mandatory = $true)]
    [string]$TrustedPin,

    [ValidateSet('windows11-ordinary', 'github-hosted')]
    [string]$AcceptanceTarget = 'windows11-ordinary',

    [string]$NodeCommand = 'node',

    [string]$PythonCommand = 'python',

    [string]$WorkingRoot,

    [switch]$KeepWorkingDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-DirectoryManifest {
    param([Parameter(Mandatory = $true)][string]$Root)

    if (-not (Test-Path -LiteralPath $Root -PathType Container)) { return @() }
    $absolute = [IO.Path]::GetFullPath($Root)
    return @(
        Get-ChildItem -LiteralPath $absolute -File -Force -Recurse |
            Sort-Object FullName |
            ForEach-Object {
                [ordered]@{
                    path = ([IO.Path]::GetRelativePath($absolute, $_.FullName) -replace '\\', '/')
                    size = $_.Length
                    sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                }
            }
    )
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
    foreach ($argument in $Arguments) { [void]$start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    if (-not $process.Start()) { throw "E_LIFECYCLE_PROCESS: unable to start $FilePath" }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill($true)
        $process.WaitForExit()
        throw "E_LIFECYCLE_TIMEOUT: $([IO.Path]::GetFileName($FilePath))"
    }
    return [ordered]@{
        file = [IO.Path]::GetFileName($FilePath)
        exitCode = $process.ExitCode
        stdout = $stdoutTask.GetAwaiter().GetResult().Trim()
        stderr = $stderrTask.GetAwaiter().GetResult().Trim()
    }
}

function Assert-Success {
    param(
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Result,
        [Parameter(Mandatory = $true)][string]$Label
    )
    if ($Result.exitCode -ne 0) {
        throw "E_LIFECYCLE_COMMAND: $Label exited $($Result.exitCode): $($Result.stderr)"
    }
}

function ConvertTo-JsonFile {
    param(
        [Parameter(Mandatory = $true)][string]$Json,
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Code
    )
    try {
        $document = $Json | ConvertFrom-Json
        $Json + "`n" | Set-Content -LiteralPath $Path -Encoding utf8NoBOM
        return $document
    }
    catch { throw "$Code`: invalid JSON output" }
}

function ConvertTo-SafeText {
    param([AllowNull()][string]$Value)

    if ($null -eq $Value) { return $null }
    $safe = [string]$Value
    $replacements = [ordered]@{
        $acceptanceRoot = '<acceptance-root>'
        $candidate = '<candidate-root>'
        $repositoryRoot = '<repository-root>'
        ([Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile)) = '<user-profile>'
    }
    foreach ($entry in $replacements.GetEnumerator()) {
        if ($entry.Key) {
            $safe = $safe.Replace([string]$entry.Key, [string]$entry.Value,
                                  [StringComparison]::OrdinalIgnoreCase)
        }
    }
    return $safe
}

function Assert-PackageFilesUnchanged {
    param(
        [Parameter(Mandatory = $true)][object[]]$Before,
        [Parameter(Mandatory = $true)][object[]]$After,
        [Parameter(Mandatory = $true)][string]$EngineSha256
    )
    $beforeByPath = @{}
    foreach ($entry in $Before) { $beforeByPath[[string]$entry.path] = $entry }
    $allowedAdditions = @(
        "runtime/$EngineSha256/engine.exe",
        "runtime/$EngineSha256/prepare.lock"
    )
    $changed = @($After | Where-Object {
        $beforeByPath.ContainsKey($_.path) -and
            ($beforeByPath[$_.path].size -ne $_.size -or $beforeByPath[$_.path].sha256 -ne $_.sha256)
    })
    $unexpected = @($After | Where-Object {
        -not $beforeByPath.ContainsKey($_.path) -and $_.path -notin $allowedAdditions
    })
    $missing = @($Before | Where-Object {
        $path = $_.path
        -not @($After | Where-Object { $_.path -eq $path })
    })
    $engine = @($After | Where-Object { $_.path -eq "runtime/$EngineSha256/engine.exe" })
    if ($changed.Count -ne 0 -or $unexpected.Count -ne 0 -or $missing.Count -ne 0 -or
        $engine.Count -ne 1 -or $engine[0].sha256 -ne $EngineSha256) {
        throw 'E_LIFECYCLE_PROGRAM: package files changed or runtime escaped the versioned allowlist'
    }
}

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$lifecycleTool = Join-Path $PSScriptRoot 'enterprise_lifecycle.py'
$builder = Join-Path $PSScriptRoot 'build_enterprise_package.py'
$platformGate = Join-Path $PSScriptRoot 'assert-windows11-x64.ps1'
$toolsFixture = Join-Path $repositoryRoot 'tests/ccode/tools-integration.py'
$resumeFixture = Join-Path $repositoryRoot 'tests/ccode/lifecycle-resume.py'
foreach ($required in @($lifecycleTool, $builder, $platformGate, $toolsFixture, $resumeFixture)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "E_LIFECYCLE_HARNESS: missing $([IO.Path]::GetFileName($required))"
    }
}

$candidate = (Resolve-Path -LiteralPath $CandidateRoot).Path
$evidenceDestination = [IO.Path]::GetFullPath($EvidencePath)
if ($WorkingRoot) {
    $acceptanceRoot = [IO.Path]::GetFullPath($WorkingRoot)
    if (Test-Path -LiteralPath $acceptanceRoot) {
        if (@(Get-ChildItem -LiteralPath $acceptanceRoot -Force).Count -ne 0) {
            throw 'E_LIFECYCLE_ROOT: working root must be absent or empty'
        }
    }
}
else {
    $acceptanceRoot = Join-Path ([IO.Path]::GetTempPath()) "ccode 企業生命週期驗收 $([Guid]::NewGuid().ToString('N'))"
}
$prefix = $acceptanceRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if ($evidenceDestination.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'E_EVIDENCE_PATH: evidence must be outside the disposable working root'
}

$inspectResult = Invoke-RecordedProcess -FilePath $PythonCommand `
    -Arguments @($lifecycleTool, 'inspect-signed', '--candidate-root', $candidate,
        '--signature', $SignaturePath, '--public-key', $PublicKeyPath,
        '--trusted-pin', $TrustedPin, '--node', $NodeCommand) `
    -CurrentDirectory $repositoryRoot
Assert-Success $inspectResult 'candidate inspect'
$inspection = $inspectResult.stdout | ConvertFrom-Json
if ($inspection.status -ne 'passed' -or $inspection.signatureVerification -ne 'passed' -or
    $inspection.platform -ne 'windows' -or
    $inspection.architecture -ne 'x64' -or $inspection.minimumWindowsBuild -ne 22000) {
    throw 'E_LIFECYCLE_CANDIDATE: candidate is not an audited Windows 11 x64 delivery'
}
if (-not (Test-Path -LiteralPath (Join-Path $candidate 'package-audit.json') -PathType Leaf)) {
    throw 'E_LIFECYCLE_CANDIDATE: package-audit.json is missing'
}

New-Item -ItemType Directory -Path $acceptanceRoot -Force | Out-Null

$unpackedCandidate = Join-Path $candidate 'unpacked'
$programRoot = Join-Path $acceptanceRoot 'portable app 中文'
$relocatedProgramRoot = Join-Path $acceptanceRoot 'portable app 搬移後 中文'
$workspaceRoot = Join-Path $acceptanceRoot 'workspace 中文 with spaces'
$dataRoot = Join-Path $acceptanceRoot 'persistent data'
$repackRoot = Join-Path $acceptanceRoot 'fresh repack output'
$platformEvidencePath = Join-Path $acceptanceRoot 'platform-evidence.json'
$provenancePath = Join-Path $acceptanceRoot 'observed-package-provenance.json'
$boundaryPath = Join-Path $acceptanceRoot 'observed-runtime-boundary.json'
New-Item -ItemType Directory -Path $workspaceRoot, $dataRoot -Force | Out-Null
Copy-Item -LiteralPath $unpackedCandidate -Destination $programRoot -Recurse
# Verify the copied bytes before executing any candidate command, including
# the platform/provenance gate. Signed source inspection alone is insufficient.
$copiedFiles = @(Get-DirectoryManifest $programRoot)
$expectedFiles = @($inspection.unpackedFiles | Sort-Object path)
$observedFiles = @($copiedFiles | Sort-Object path)
if ($expectedFiles.Count -ne $observedFiles.Count) { throw 'E_LIFECYCLE_COPY' }
for ($index = 0; $index -lt $expectedFiles.Count; $index++) {
    if ($expectedFiles[$index].path -cne $observedFiles[$index].path -or
        $expectedFiles[$index].size -ne $observedFiles[$index].size -or
        $expectedFiles[$index].sha256 -cne $observedFiles[$index].sha256) {
        throw 'E_LIFECYCLE_COPY'
    }
}
# Detached signature is an installed control file, not a fresh-package entry.
# Verify a bounded snapshot and install exclusively before any candidate command.
$installResult = Invoke-RecordedProcess -FilePath $PythonCommand `
    -Arguments @($lifecycleTool, 'install-signature', '--program-root', $programRoot,
        '--signature', $SignaturePath, '--public-key', $PublicKeyPath,
        '--trusted-pin', $TrustedPin, '--node', $NodeCommand) `
    -CurrentDirectory $repositoryRoot
Assert-Success $installResult 'detached signature installation'
$signatureInstallation = $installResult.stdout | ConvertFrom-Json
if ($signatureInstallation.status -ne 'passed' -or
    $signatureInstallation.signatureVerification -ne 'passed' -or
    $signatureInstallation.signedManifestSha256 -cne $inspection.signedManifestSha256) {
    throw 'E_LIFECYCLE_INSTALL'
}
$app = Join-Path $programRoot 'ccode.exe'
$manifest = $inspection.manifest

& $platformGate -Executable $app -ExpectedEngineVersion $manifest.provenance.engineVersion `
    -AdapterRevision $manifest.provenance.adapterRevision -EvidencePath $platformEvidencePath `
    -AcceptanceTarget $AcceptanceTarget | Out-Null
$platformEvidence = Get-Content -LiteralPath $platformEvidencePath -Raw | ConvertFrom-Json

$programBefore = Get-DirectoryManifest $programRoot
$failure = $null
$workspaceIdentityBefore = $null
$workspaceIdentityAfter = $null
$sessionsBeforeMove = $null
$resumeEvidence = $null
$repackComparison = $null

try {
    $packageResult = Invoke-RecordedProcess -FilePath $app -Arguments @('--package-manifest') `
        -CurrentDirectory $workspaceRoot
    Assert-Success $packageResult 'package manifest'
    $observedPackage = ConvertTo-JsonFile -Json $packageResult.stdout -Path $provenancePath `
        -Code 'E_PACKAGE_METADATA'
    if ($observedPackage.platform -ne 'windows' -or $observedPackage.architecture -ne 'x64' -or
        $observedPackage.engineVersion -ne $manifest.provenance.engineVersion -or
        $observedPackage.engineSha256 -ne $manifest.provenance.engineSha256 -or
        $observedPackage.adapterRevision -ne $manifest.provenance.adapterRevision) {
        throw 'E_PACKAGE_METADATA: executable identity differs from enterprise manifest'
    }

    $boundaryResult = Invoke-RecordedProcess -FilePath $app -Arguments @('--boundary-manifest') `
        -CurrentDirectory $workspaceRoot
    Assert-Success $boundaryResult 'boundary manifest'
    $observedBoundary = ConvertTo-JsonFile -Json $boundaryResult.stdout -Path $boundaryPath `
        -Code 'E_BOUNDARY'
    if ($observedBoundary.platform -ne 'windows' -or $observedBoundary.architecture -ne 'x64' -or
        $observedBoundary.minimumWindowsBuild -ne 22000) {
        throw 'E_BOUNDARY: executable boundary is not Windows 11 x64'
    }

    $identityResult = Invoke-RecordedProcess -FilePath $app `
        -Arguments @('--data-dir', $dataRoot, '--workspace-id') -CurrentDirectory $workspaceRoot
    Assert-Success $identityResult 'workspace identity before move'
    $workspaceIdentityBefore = $identityResult.stdout

    # This deterministic loopbackFixture replaces only the model response. The real
    # engine, frontend and six tools execute; it is not a live model or enterprise gateway.
    $toolsResult = Invoke-RecordedProcess -FilePath $PythonCommand `
        -Arguments @($toolsFixture, $app, '--acceptance-root', $acceptanceRoot) `
        -CurrentDirectory $repositoryRoot
    Assert-Success $toolsResult 'real engine and six tools'

    $sessionsResult = Invoke-RecordedProcess -FilePath $app `
        -Arguments @('--data-dir', $dataRoot, '--sessions') -CurrentDirectory $workspaceRoot
    Assert-Success $sessionsResult 'sessions before move'
    if ($sessionsResult.stdout -match 'No saved sessions') {
        throw 'E_LIFECYCLE_HISTORY: tool trial did not preserve a resumable session'
    }
    $sessionsBeforeMove = $sessionsResult.stdout

    $programAfterRun = Get-DirectoryManifest $programRoot
    Assert-PackageFilesUnchanged -Before $programBefore -After $programAfterRun `
        -EngineSha256 $manifest.provenance.engineSha256
    foreach ($excludedDynamicData in @('data', 'profile', 'sessions', 'temp')) {
        if (Test-Path -LiteralPath (Join-Path $programRoot $excludedDynamicData)) {
            throw "E_LIFECYCLE_DATA: excludedDynamicData appeared inside the program directory"
        }
    }
    if (@(Get-DirectoryManifest $dataRoot).Count -eq 0) {
        throw 'E_LIFECYCLE_DATA: external persistent data was not created'
    }

    Move-Item -LiteralPath $programRoot -Destination $relocatedProgramRoot
    $app = Join-Path $relocatedProgramRoot 'ccode.exe'
    $identityAfterResult = Invoke-RecordedProcess -FilePath $app `
        -Arguments @('--data-dir', $dataRoot, '--workspace-id') -CurrentDirectory $workspaceRoot
    Assert-Success $identityAfterResult 'workspace identity after move'
    $workspaceIdentityAfter = $identityAfterResult.stdout
    if ($workspaceIdentityBefore -ne $workspaceIdentityAfter) {
        throw 'E_LIFECYCLE_IDENTITY: complete program relocation changed workspace identity'
    }

    $resumeResult = Invoke-RecordedProcess -FilePath $PythonCommand `
        -Arguments @($resumeFixture, $app, $workspaceRoot, $dataRoot, $workspaceIdentityBefore) `
        -CurrentDirectory $repositoryRoot
    Assert-Success $resumeResult 'history resume after move'
    $resumeEvidence = $resumeResult.stdout | ConvertFrom-Json

    $builderArguments = [Collections.Generic.List[string]]::new()
    foreach ($value in @('--executable', $app, '--provenance', $provenancePath,
                         '--boundary', $boundaryPath, '--usage',
                         (Join-Path $unpackedCandidate $inspection.usagePath))) {
        [void]$builderArguments.Add([string]$value)
    }
    foreach ($notice in @($manifest.notices)) {
        [void]$builderArguments.Add('--notice')
        [void]$builderArguments.Add((Join-Path $unpackedCandidate $notice.path))
        [void]$builderArguments.Add('--notice-sha256')
        [void]$builderArguments.Add([string]$notice.sha256)
    }
    foreach ($name in @($inspection.restrictedNames)) {
        [void]$builderArguments.Add('--restricted-name')
        [void]$builderArguments.Add([string]$name)
    }
    [void]$builderArguments.Add('--output')
    [void]$builderArguments.Add($repackRoot)
    $repackResult = Invoke-RecordedProcess -FilePath $PythonCommand -Arguments $builderArguments.ToArray() `
        -CurrentDirectory $repositoryRoot
    Assert-Success $repackResult 'fresh enterprise repack'

    $compareResult = Invoke-RecordedProcess -FilePath $PythonCommand `
        -Arguments @($lifecycleTool, 'compare', '--original', $candidate, '--repacked', $repackRoot) `
        -CurrentDirectory $repositoryRoot
    Assert-Success $compareResult 'original and fresh repack comparison'
    $repackComparison = $compareResult.stdout | ConvertFrom-Json
}
catch {
    $failure = $_.Exception.Message
}

$programFinal = Get-DirectoryManifest $relocatedProgramRoot
$dataFinal = Get-DirectoryManifest $dataRoot
$workspaceFinal = Get-DirectoryManifest $workspaceRoot
$evidence = [ordered]@{
    schemaVersion = 1
    collectedAtUtc = [DateTime]::UtcNow.ToString('o')
    acceptance = [ordered]@{
        scope = $(if ($AcceptanceTarget -eq 'github-hosted') {
            'GitHub-hosted Windows x64 complete enterprise lifecycle trial'
        } else { 'Windows 11 x64 ordinary-account complete enterprise lifecycle trial' })
        acceptanceTarget = $AcceptanceTarget
        passed = -not [bool]$failure
        failure = ConvertTo-SafeText $failure
        loopbackFixture = [ordered]@{
            used = $true
            limitation = 'deterministic local response fixture; not a live model or enterprise gateway'
        }
    }
    platform = $platformEvidence.platform
    account = $platformEvidence.account
    candidate = [ordered]@{
        archive = $inspection.archive
        archiveSha256 = $inspection.archiveSha256
        signatureVerification = $inspection.signatureVerification
        signedManifestSha256 = $inspection.signedManifestSha256
        installedSignatureSha256 = $signatureInstallation.installedSignatureSha256
        audit = [ordered]@{ status = 'passed'; comparison = 'matched' }
        manifest = $manifest
        packageFilesBefore = $programBefore
    }
    lifecycle = [ordered]@{
        workspaceIdentityBefore = $workspaceIdentityBefore
        workspaceIdentityAfter = $workspaceIdentityAfter
        historyListedBeforeMove = [bool]$sessionsBeforeMove
        resumedAfterMove = $resumeEvidence
        programFilesAfterMoveAndResume = $programFinal
        externalDataFiles = $dataFinal
        workspaceFiles = $workspaceFinal
    }
    repack = $repackComparison
    packagePolicy = [ordered]@{
        excludedDynamicData = @('data/', 'profile/', 'runtime/', 'sessions/', 'temp/')
        runtimeWasExcludedFromFreshRepack = [bool]($repackComparison -and $repackComparison.comparison -eq 'matched')
    }
}
$evidenceParent = Split-Path -Parent $evidenceDestination
if ($evidenceParent) { New-Item -ItemType Directory -Path $evidenceParent -Force | Out-Null }
$evidence | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $evidenceDestination -Encoding utf8NoBOM

if (-not $KeepWorkingDirectory) { Remove-Item -LiteralPath $acceptanceRoot -Recurse -Force }
if ($failure) { throw $failure }
Write-Output "Enterprise lifecycle acceptance passed ($AcceptanceTarget). Evidence: $evidenceDestination"
