param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$AdapterRevision,

    [Parameter(Mandatory = $false)]
    [string]$OutputDir = ".",

    # Paired explicit public trust inputs. Omitting both builds the public edition.
    [string]$SignerSpkiPath = "",
    [string]$ApprovedSignerPin = ""
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$baseUrl = "https://storage.googleapis.com/claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/claude-code-releases/$Version"
$officialManifestUrl = "$baseUrl/manifest.json"
$officialPayloadUrl = "$baseUrl/win32-x64/claude.exe"
if ($AdapterRevision -notmatch '^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$') {
    throw "AdapterRevision must be a full 40- or 64-character hexadecimal commit ID"
}
if ([bool]$SignerSpkiPath -ne [bool]$ApprovedSignerPin) { throw "E_SIGNER_POLICY" }
if ($SignerSpkiPath) { $SignerSpkiPath = (Resolve-Path -LiteralPath $SignerSpkiPath).Path }
$normalizedAdapterRevision = $AdapterRevision.ToLowerInvariant()
$work = Join-Path ([System.IO.Path]::GetTempPath()) "ccode-build-$([Guid]::NewGuid().ToString('N'))"
$payload = Join-Path $work "aa-runtime.exe"
$metadataPath = Join-Path $work "package.json"
$manifestPath = Join-Path $work "manifest.json"
$resourceScript = Join-Path $work "ccode.rc"
$resourceObject = Join-Path $work "ccode.res"
$output = Join-Path (Resolve-Path $OutputDir) "ccode.exe"
$locationPushed = $false

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE"
    }
}

try {
    New-Item -ItemType Directory -Path $work | Out-Null
    Push-Location $work
    $locationPushed = $true

    Write-Output "Downloading and verifying official payload $Version..."
    Invoke-WebRequest -Uri $officialManifestUrl -OutFile $manifestPath -UseBasicParsing
    Invoke-WebRequest -Uri $officialPayloadUrl -OutFile $payload -UseBasicParsing
    $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
    $expected = [string]$manifest.platforms.'win32-x64'.checksum
    $actual = (Get-FileHash -Path $payload -Algorithm SHA256).Hash.ToLowerInvariant()
    $officialManifestSha256 = (Get-FileHash -Path $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $engineSize = (Get-Item $payload).Length
    if (-not $expected -or $actual -ne $expected.ToLowerInvariant()) {
        throw "Official payload SHA256 mismatch. Expected $expected, got $actual"
    }

    Write-Output "Embedding verified engine provenance..."
    @{
        schemaVersion = 1
        packageName = "ccode"
        packageVersion = "1.0"
        platform = "windows"
        architecture = "x64"
        adapterRevision = $normalizedAdapterRevision
        engineVersion = $Version
        engineSha256 = $actual
        engineSize = $engineSize
        officialManifestUrl = $officialManifestUrl
        officialManifestSha256 = $officialManifestSha256
        officialPayloadUrl = $officialPayloadUrl
    } | ConvertTo-Json -Compress | Set-Content -Path $metadataPath -Encoding utf8NoBOM
    $payloadRc = $payload.Replace('\', '\\')
    $metadataRc = $metadataPath.Replace('\', '\\')
    @"
101 RCDATA "$payloadRc"
102 RCDATA "$metadataRc"
"@ | Set-Content -Path $resourceScript -Encoding ASCII
    Invoke-Checked rc.exe /nologo "/fo$resourceObject" $resourceScript

    $cryptoObjects = @()
    foreach ($unit in @("monocypher", "monocypher-ed25519")) {
        $object = Join-Path $work "$unit.obj"
        Invoke-Checked cl.exe /nologo /O2 /MT /c /TC `
            (Join-Path $PSScriptRoot "vendor\monocypher\$unit.c") "/Fo:$object"
        $cryptoObjects += $object
    }

    $policyGenerator = Join-Path $PSScriptRoot "generate_enterprise_policy.py"
    $enterpriseFlags = @()
    $launcherObjects = @()
    if ($SignerSpkiPath) {
        Invoke-Checked python $policyGenerator --spki $SignerSpkiPath --approved-pin $ApprovedSignerPin `
            --output (Join-Path $work "enterprise-policy.hpp")
        $enterpriseFlags = @("/DCCODE_ENTERPRISE_REQUIRED", "/I$work")
        $launcherObjects = $cryptoObjects
    }
    $launcherArgs = @(
        "/nologo", "/std:c++17", "/O2", "/EHsc", "/MT", "/utf-8", "/DUNICODE", "/D_UNICODE",
        (Join-Path $PSScriptRoot "launcher.cpp"), $resourceObject
    ) + $enterpriseFlags + $launcherObjects + @(
        "/Fe:$output", "/link", "bcrypt.lib", "winhttp.lib", "ws2_32.lib", "/SUBSYSTEM:CONSOLE"
    )
    Invoke-Checked cl.exe @launcherArgs

    foreach ($suite in @("gateway-http", "process-tree", "native", "profile", "environment", "frontend", "session", "permission", "diagnostic")) {
        $testExe = Join-Path $work "ccode-$suite-tests.exe"
        Invoke-Checked cl.exe /nologo /std:c++17 /O2 /EHsc /MT /utf-8 `
            (Join-Path $PSScriptRoot "..\..\tests\ccode\$suite-tests.cpp") "/Fe:$testExe" /link bcrypt.lib
        Invoke-Checked $testExe
    }

    $gatewayTrustTest = Join-Path $work "ccode-gateway-trust-tests.exe"
    Invoke-Checked cl.exe /nologo /std:c++17 /O2 /EHsc /MT /utf-8 `
        (Join-Path $PSScriptRoot "..\..\tests\ccode\gateway-trust-tests.cpp") `
        "/Fe:$gatewayTrustTest" /link crypt32.lib
    Invoke-Checked $gatewayTrustTest (Join-Path $PSScriptRoot "..\..\tests\ccode\fixtures")

    $gatewayProbe = Join-Path $work "ccode-gateway-bridge-probe.exe"
    Invoke-Checked cl.exe /nologo /std:c++17 /O2 /EHsc /MT /utf-8 `
        (Join-Path $PSScriptRoot "..\..\tests\ccode\gateway-bridge-probe.cpp") `
        "/Fe:$gatewayProbe" /link bcrypt.lib winhttp.lib ws2_32.lib
    Invoke-Checked python (Join-Path $PSScriptRoot "..\..\tests\ccode\gateway-bridge-integration.py") $gatewayProbe

    # Native verifier engineering tests. No Node runtime is linked into ccode.
    # Approved real signer policy and lifecycle integration remain pending.
    $signatureTest = Join-Path $work "ccode-signature-tests.exe"
    Invoke-Checked cl.exe /nologo /std:c++17 /O2 /EHsc /MT /utf-8 `
        (Join-Path $PSScriptRoot "..\..\tests\ccode\signature-tests.cpp") `
        @cryptoObjects "/Fe:$signatureTest" /link bcrypt.lib
    Invoke-Checked $signatureTest $output $metadataPath

    # Independent public RFC8032 fixture: used only for an ephemeral test executable.
    # Never used as the output edition's trust policy or as approval evidence.
    $fixtureDir = Join-Path $work "fixture-policy"
    New-Item -ItemType Directory -Path $fixtureDir | Out-Null
    $fixtureKey = Join-Path $fixtureDir "public.der"
    [IO.File]::WriteAllBytes($fixtureKey, [Convert]::FromHexString(
        "302a300506032b6570032100d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a"))
    Invoke-Checked python $policyGenerator --spki $fixtureKey `
        --approved-pin "06e3fd8fda29bb60ab59557de61edb0aecdb231134be30e75b455f8e1b792fa9" `
        --output (Join-Path $fixtureDir "enterprise-policy.hpp")
    $fixtureExe = Join-Path $work "enterprise-startup.exe"
    $fixtureArgs = @("/nologo", "/std:c++17", "/O2", "/EHsc", "/MT", "/utf-8",
        "/DUNICODE", "/D_UNICODE", "/DCCODE_ENTERPRISE_REQUIRED", "/I$fixtureDir",
        (Join-Path $PSScriptRoot "launcher.cpp"), $resourceObject) + $cryptoObjects + @(
        "/Fe:$fixtureExe", "/link", "bcrypt.lib", "winhttp.lib", "ws2_32.lib", "/SUBSYSTEM:CONSOLE")
    Invoke-Checked cl.exe @fixtureArgs
    Invoke-Checked $signatureTest $fixtureExe $metadataPath --enterprise-launcher
    & (Join-Path $PSScriptRoot "..\..\tests\ccode\test-enterprise-startup.ps1") -Executable $fixtureExe

    $info = Get-Item $output
    if ($info.Length -le (Get-Item $payload).Length) {
        throw "ccode.exe is unexpectedly small; embedded resources may be missing"
    }
    # Export inputs for signing/assembly without invoking the unsigned enterprise
    # launcher. Provenance is the exact buffer embedded as resource102; boundary
    # comes from the same C++ document used by the launcher, not a copied schema.
    $boundaryExporter = Join-Path $work "export-build-boundary.exe"
    Invoke-Checked cl.exe /nologo /std:c++17 /O2 /EHsc /MT /utf-8 `
        (Join-Path $PSScriptRoot "export-build-boundary.cpp") "/Fe:$boundaryExporter"
    $boundaryJson = & $boundaryExporter
    if ($LASTEXITCODE -ne 0) { throw "E_BUILD_BOUNDARY" }
    $sidecarDirectory = Split-Path -Parent $output
    $boundaryJson | Set-Content -LiteralPath (Join-Path $sidecarDirectory "runtime-boundary.json") -Encoding utf8NoBOM
    Copy-Item -LiteralPath $metadataPath -Destination (Join-Path $sidecarDirectory "package-provenance.json")
    Write-Output "Built single-file ccode.exe ($($info.Length) bytes)."
}
finally {
    if ($locationPushed) {
        Pop-Location
    }
    if (Test-Path $work) {
        Remove-Item $work -Recurse -Force
    }
}
