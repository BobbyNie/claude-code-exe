# Exercise the actual inventory helper without running platform/network operations.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = Join-Path $PSScriptRoot '../../scripts/ccode/accept-offline-windows11-x64.ps1'
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($source, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Offline harness parse failed' }
$function = $ast.Find({ param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
    $node.Name -eq 'Get-StringSha256'
}, $true)
if (-not $function) { throw 'Missing inventory hash helper' }
. ([scriptblock]::Create($function.Extent.Text))

# Hidden/disabled adapters may have an empty display field. Its digest is valid,
# while the adapter and its connectivity state must remain in the inventory.
if ((Get-StringSha256 '') -ne 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855') {
    throw 'Empty field digest mismatch'
}
if ((Get-StringSha256 'abc') -ne 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad') {
    throw 'Nonempty field digest mismatch'
}
Write-Output 'PASS: offline inventory field hashing'
