param([Parameter(Mandatory = $true)][string]$Executable)
$ErrorActionPreference = 'Stop'
# Real subprocess test of the enterprise build, not an unsigned public launcher.
$root = Join-Path ([IO.Path]::GetTempPath()) ('ccode-startup-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $root | Out-Null
try {
    $program = Join-Path $root 'program'
    New-Item -ItemType Directory $program | Out-Null
    $exe = Join-Path $program 'ccode.exe'
    Copy-Item -LiteralPath $Executable -Destination $exe
    foreach ($arguments in @('--help', '--version', '--package-manifest', '--boundary-manifest',
                              '--ccode-self-test', '--ccode-permission-server', '--print probe')) {
        $stdout = Join-Path $root 'stdout.txt'
        $stderr = Join-Path $root 'stderr.txt'
        $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $root `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
        if (-not $process.WaitForExit(15000)) {
            $process.Kill(); $process.WaitForExit()
            throw "Enterprise unsigned startup did not terminate: $arguments"
        }
        $process.Refresh()
        if ($process.ExitCode -ne 64 -or (Get-Content $stderr -Raw).Trim() -ne 'E_MANIFEST_FILE') {
            throw "Enterprise unsigned startup did not fail closed: $arguments"
        }
        if ((Get-Item $stdout).Length -ne 0) { throw 'Unauthenticated entry point produced output' }
        if (@(Get-ChildItem -LiteralPath $program -Force).Count -ne 1) {
            throw 'Unauthenticated entry point changed program directory'
        }
    }
    Write-Output 'Enterprise missing-signature entry-point refusal passed.'
}
finally { Remove-Item -LiteralPath $root -Recurse -Force }
