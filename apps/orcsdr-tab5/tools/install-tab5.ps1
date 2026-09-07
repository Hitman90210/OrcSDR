param(
  [Parameter(Mandatory = $true)]
  [ValidatePattern('^COM[0-9]+$')]
  [string]$Port,
  [string]$IdfPath = 'C:\Espressif\v5.5.4\esp-idf'
)

$ErrorActionPreference = 'Stop'
$rootInstaller = Join-Path $PSScriptRoot '..\..\..\install-orcsdr.ps1'
Write-Host 'Delegating to the guarded root installer (explicit bootloader, partition table, and app offsets; NVS preserved).'
& $rootInstaller -Port $Port -IdfPath $IdfPath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
