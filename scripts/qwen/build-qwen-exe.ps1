# Build a self-contained qwen.exe from the extracted official standalone package.

param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $false)]
    [string]$SourceDir = "qwen-code",

    [Parameter(Mandatory = $false)]
    [string]$OutputDir = "."
)

$ErrorActionPreference = "Stop"

$runtimeZip = Join-Path $OutputDir "qwen-runtime.zip"
$outputExe = Join-Path $OutputDir "qwen.exe"
$versionFile = Join-Path $OutputDir "qwen-version.txt"
$launcherSource = Join-Path $PSScriptRoot "launcher.cs"

if (-not (Test-Path (Join-Path $SourceDir "bin\qwen.cmd"))) {
    Write-Error "Source directory is missing bin\qwen.cmd: $SourceDir"
}

if (-not (Test-Path $launcherSource)) {
    Write-Error "Launcher source not found: $launcherSource"
}

Write-Output "=== Build qwen.exe ==="
Write-Output "Version: $Version"

Set-Content -Path $versionFile -Value $Version -Encoding ASCII -NoNewline

if (Test-Path $runtimeZip) {
    Remove-Item $runtimeZip -Force
}

Write-Output "Creating embedded runtime archive..."
Compress-Archive -Path (Join-Path $SourceDir "*") -DestinationPath $runtimeZip -Force

$cscCandidates = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\Roslyn\csc.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\Roslyn\csc.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\Roslyn\csc.exe",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\Roslyn\csc.exe",
    "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
)

$cscPath = $null
foreach ($candidate in $cscCandidates) {
    if (Test-Path $candidate) {
        $cscPath = $candidate
        break
    }
}

if (-not $cscPath) {
    Write-Error "C# compiler (csc.exe) not found on this runner"
}

Write-Output "Using compiler: $cscPath"

if (Test-Path $outputExe) {
    Remove-Item $outputExe -Force
}

$compileArgs = @(
    "/nologo",
    "/target:exe",
    "/optimize+",
    "/out:$outputExe",
    "/reference:System.IO.Compression.dll",
    "/reference:System.IO.Compression.FileSystem.dll",
    "/resource:$runtimeZip,QwenRuntime",
    "/resource:$versionFile,QwenVersion",
    $launcherSource
)

& $cscPath @compileArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to compile qwen.exe"
}

Remove-Item $runtimeZip -Force
Remove-Item $versionFile -Force

$exeInfo = Get-Item $outputExe
Write-Output "Built qwen.exe: $($exeInfo.Length) bytes ($([math]::Round($exeInfo.Length / 1MB, 2)) MB)"

if ($exeInfo.Length -lt 10MB) {
    Write-Error "qwen.exe is unexpectedly small; embedded runtime may be missing"
}

Write-Output "qwen.exe build complete!"
