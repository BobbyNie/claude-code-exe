param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$AdapterRevision,

    [Parameter(Mandatory = $false)]
    [string]$OutputDir = "."
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$baseUrl = "https://storage.googleapis.com/claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/claude-code-releases/$Version"
$officialManifestUrl = "$baseUrl/manifest.json"
$officialPayloadUrl = "$baseUrl/win32-x64/claude.exe"
if ($AdapterRevision -notmatch '^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$') {
    throw "AdapterRevision must be a full 40- or 64-character hexadecimal commit ID"
}
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

    $launcherArgs = @(
        "/nologo", "/std:c++17", "/O2", "/EHsc", "/MT", "/utf-8", "/DUNICODE", "/D_UNICODE",
        (Join-Path $PSScriptRoot "launcher.cpp"), $resourceObject,
        "/Fe:$output", "/link", "bcrypt.lib", "/SUBSYSTEM:CONSOLE"
    )
    Invoke-Checked cl.exe @launcherArgs

    foreach ($suite in @("native", "profile", "environment", "frontend", "session", "permission", "diagnostic")) {
        $testExe = Join-Path $work "ccode-$suite-tests.exe"
        Invoke-Checked cl.exe /nologo /std:c++17 /O2 /EHsc /MT /utf-8 `
            (Join-Path $PSScriptRoot "..\..\tests\ccode\$suite-tests.cpp") "/Fe:$testExe"
        Invoke-Checked $testExe
    }

    # Native verifier engineering tests. No Node runtime is linked into ccode.
    # Production startup/update integration and approved trust policy remain pending.
    $cryptoObjects = @()
    foreach ($unit in @("monocypher", "monocypher-ed25519")) {
        $object = Join-Path $work "$unit.obj"
        Invoke-Checked cl.exe /nologo /O2 /MT /c /TC `
            (Join-Path $PSScriptRoot "vendor\monocypher\$unit.c") "/Fo:$object"
        $cryptoObjects += $object
    }
    $signatureTest = Join-Path $work "ccode-signature-tests.exe"
    Invoke-Checked cl.exe /nologo /std:c++17 /O2 /EHsc /MT /utf-8 `
        (Join-Path $PSScriptRoot "..\..\tests\ccode\signature-tests.cpp") `
        @cryptoObjects "/Fe:$signatureTest"
    Invoke-Checked $signatureTest

    $info = Get-Item $output
    if ($info.Length -le (Get-Item $payload).Length) {
        throw "ccode.exe is unexpectedly small; embedded resources may be missing"
    }
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
