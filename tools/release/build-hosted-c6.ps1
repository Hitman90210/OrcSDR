#requires -Version 5.1
<# Builds the release-owned ESP-Hosted C6 image; it never flashes hardware. #>
param(
  [Parameter(Mandatory)] [string]$OutputDirectory,
  [string]$IdfPath = 'C:\Espressif\v5.5.4\esp-idf',
  [string]$SourceDirectory
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

function Get-Sha256([string]$Path) {
  $sha = [Security.Cryptography.SHA256]::Create()
  $stream = [IO.File]::OpenRead($Path)
  try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
  finally { $stream.Dispose(); $sha.Dispose() }
}

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$lock = Get-Content (Join-Path $PSScriptRoot 'hosted-c6-release.json') -Raw | ConvertFrom-Json
if (-not $SourceDirectory) {
  $SourceDirectory = Join-Path $env:TEMP "OrcSDR-esp-hosted-$($lock.hosted_version)"
}
if (-not (Test-Path (Join-Path $IdfPath 'export.ps1'))) { throw "ESP-IDF 5.5.4 is required at $IdfPath." }

if (-not (Test-Path (Join-Path $SourceDirectory '.git'))) {
  git clone $lock.source_repository $SourceDirectory
  if ($LASTEXITCODE) { throw 'Could not clone the pinned Espressif ESP-Hosted source.' }
}
git -C $SourceDirectory fetch origin $lock.source_revision
if ($LASTEXITCODE) { throw 'Could not fetch the pinned ESP-Hosted revision.' }
git -C $SourceDirectory checkout --detach $lock.source_revision
if ($LASTEXITCODE) { throw 'Could not check out the pinned ESP-Hosted revision.' }
if ((git -C $SourceDirectory rev-parse HEAD).Trim() -ne $lock.source_revision) { throw 'ESP-Hosted revision mismatch.' }
git -C $SourceDirectory submodule update --init --recursive
if ($LASTEXITCODE) { throw 'Could not initialize the pinned ESP-Hosted submodules.' }

$project = Join-Path $SourceDirectory $lock.source_project
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$build = 'build-orcsdr-tab5-c6'
$componentRoot = Join-Path $SourceDirectory '.orcsdr-components'
$componentLink = Join-Path $componentRoot 'esp_hosted'
if (Test-Path $componentLink) {
  if (-not ((Get-Item -LiteralPath $componentLink).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Refusing to remove non-link component path: $componentLink" }
  [IO.Directory]::Delete($componentLink)
}
New-Item -ItemType Directory -Force -Path $componentRoot | Out-Null
New-Item -ItemType Junction -Path $componentLink -Target $SourceDirectory | Out-Null
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
Use-IdfEnvironment
Push-Location $project
try {
  idf.py -B $build -D "EXTRA_COMPONENT_DIRS=$componentRoot" set-target esp32c6
  if ($LASTEXITCODE) { throw 'ESP-Hosted C6 target configuration failed.' }
  idf.py -B $build -D "EXTRA_COMPONENT_DIRS=$componentRoot" build
  if ($LASTEXITCODE) { throw 'ESP-Hosted C6 build failed.' }
  $sourceImage = Join-Path $project "$build\eh_cp_wifi_scan.bin"
  if (-not (Test-Path $sourceImage)) { throw "Missing C6 application image: $sourceImage" }
  New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
  $image = Join-Path $OutputDirectory $lock.output_name
  Copy-Item $sourceImage $image -Force
} finally { Pop-Location }

$hash = Get-Sha256 $image
[ordered]@{
  hosted_version = $lock.hosted_version
  source_repository = $lock.source_repository
  source_revision = $lock.source_revision
  source_project = $lock.source_project
  target = $lock.target
  transport = $lock.transport
  board_configuration = 'M5Stack Tab5 internal ESP32-C6; P4 host uses ESP32P4_TAB5_C6_BOARD and qualified 4-bit SDIO at 10 MHz'
  idf_version = $lock.idf_version
  toolchain = (& riscv32-esp-elf-gcc --version | Select-Object -First 1)
  sdkconfig_sha256 = Get-Sha256 (Join-Path $project 'sdkconfig')
  firmware = (Split-Path $image -Leaf)
  bytes = (Get-Item $image).Length
  sha256 = $hash
} | ConvertTo-Json | Set-Content (Join-Path $OutputDirectory 'c6-provenance.json')
Write-Host "HOSTED_C6_BUILD_OK version=$($lock.hosted_version) bytes=$((Get-Item $image).Length) sha256=$hash"
