#requires -Version 5.1
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script = Join-Path $root 'install-orcsdr.ps1'
$hosted = (Get-Content (Join-Path $PSScriptRoot 'hosted-c6-release.json') -Raw | ConvertFrom-Json).hosted_version
$matched = "RTL_WIFI_HOSTED host=$hosted coprocessor=$hosted match=1"
try {
  & $script -MockHostedLine $matched | Out-Null
  throw 'MockHostedLine without DryRun was accepted.'
} catch {
  if ($_.Exception.Message -notmatch 'MockHostedLine is allowed only with -DryRun') { throw }
}
& $script -DryRun -MockHostedLine $matched | Out-Host
if ($LASTEXITCODE) { throw 'Matched-pair dry run failed.' }
& $script -DryRun -MockHostedLine "RTL_WIFI_HOSTED host=$hosted coprocessor=0.0.0 match=0" | Out-Host
if ($LASTEXITCODE) { throw 'Mismatch-without-update dry run should still flash final P4.' }
& $script -DryRun -UpdateC6 -MockHostedLine $matched | Out-Host
if ($LASTEXITCODE) { throw 'Guarded C6 update dry run failed.' }
Write-Host 'INSTALLER_DRY_RUN_OK'
