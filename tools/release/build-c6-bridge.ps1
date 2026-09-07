#requires -Version 5.1
<# Builds the temporary P4 bridge around a reproducibly built C6 image. #>
param(
  [Parameter(Mandatory)] [string]$C6Firmware,
  [Parameter(Mandatory)] [string]$OutputDirectory,
  [string]$IdfPath = 'C:\Espressif\v5.5.4\esp-idf'
)
$ErrorActionPreference = 'Stop'
function Use-IdfEnvironment {
  $idfInstallRoot = Split-Path -Parent (Split-Path -Parent $IdfPath)
  $env:IDF_TOOLS_PATH = Join-Path $idfInstallRoot 'tools'
  $pythonEnvCandidates = @(
    (Join-Path $idfInstallRoot 'tools\python\v5.5.4\venv'),
    'C:\Espressif\python_env\idf5.5_py3.14_env'
  )
  $resolvedPythonEnv = $pythonEnvCandidates |
    Where-Object { Test-Path -LiteralPath (Join-Path $_ 'Scripts\python.exe') -PathType Leaf } |
    Select-Object -First 1
  if (-not $resolvedPythonEnv) {
    throw "Unable to locate the ESP-IDF 5.5.4 Python environment. Checked: $($pythonEnvCandidates -join ', ')"
  }
  $env:IDF_PYTHON_ENV_PATH = $resolvedPythonEnv
  $ccache = Get-ChildItem -LiteralPath (Join-Path $env:IDF_TOOLS_PATH 'ccache') `
    -Filter ccache.exe -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1
  $toolPaths = @($env:IDF_PYTHON_ENV_PATH + '\Scripts')
  if ($ccache) { $toolPaths += $ccache.DirectoryName }
  $env:PATH = ($toolPaths -join ';') + ";$env:PATH"
  $eimProfile = Join-Path $env:IDF_TOOLS_PATH 'Microsoft.v5.5.4.PowerShell_profile.ps1'
  if (Test-Path -LiteralPath $eimProfile -PathType Leaf) { . $eimProfile }
  else { . (Join-Path $IdfPath 'export.ps1') }
  if ($ccache) { $env:PATH = "$($ccache.DirectoryName);$env:PATH" }
  $global:LASTEXITCODE = 0
}
if (-not (Test-Path $C6Firmware)) { throw "Missing C6 firmware: $C6Firmware" }
$C6Firmware = (Resolve-Path $C6Firmware).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$lock = Get-Content (Join-Path $PSScriptRoot 'hosted-c6-release.json') -Raw | ConvertFrom-Json
if ($lock.hosted_version -notmatch '^\d+\.\d+\.\d+$') {
  throw 'Pinned Hosted version is missing or invalid.'
}
$app = Join-Path $repo 'apps\orcsdr-c6-bridge'
$build = 'build-release-bridge'
$env:PYTHONUTF8 = '1'; $env:PYTHONIOENCODING = 'utf-8'
Use-IdfEnvironment
Push-Location $app
try {
  New-Item -ItemType Directory -Force -Path $build | Out-Null
  Copy-Item sdkconfig.defaults (Join-Path $build 'sdkconfig') -Force
  idf.py -B $build -D "SDKCONFIG=$build/sdkconfig" -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults' -D "C6_FIRMWARE_BIN=$C6Firmware" -D "C6_HOSTED_VERSION=$($lock.hosted_version)" reconfigure
  if ($LASTEXITCODE) { throw 'C6 bridge configuration failed.' }
  idf.py -B $build build
  if ($LASTEXITCODE) { throw 'C6 bridge build failed.' }
  idf.py -B $build merge-bin --format raw --output (Join-Path $OutputDirectory 'OrcSDR-Hosted-Bridge.bin')
  if ($LASTEXITCODE) { throw 'C6 bridge merge-bin failed.' }
  Copy-Item (Join-Path $build 'orcsdr_c6_bridge.bin') (Join-Path $OutputDirectory 'orcsdr_c6_bridge.bin') -Force
  Copy-Item (Join-Path $build 'bootloader\bootloader.bin') (Join-Path $OutputDirectory 'bootloader_0x2000.bin') -Force
  Copy-Item (Join-Path $build 'partition_table\partition-table.bin') (Join-Path $OutputDirectory 'partition-table_0x8000.bin') -Force
} finally { Pop-Location }
Write-Host "C6_BRIDGE_BUILD_OK output=$OutputDirectory"
