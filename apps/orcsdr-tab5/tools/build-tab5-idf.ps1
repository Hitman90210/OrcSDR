param(
  [string]$IdfPath = 'C:\Espressif\v5.5.4\esp-idf',
  # Defaults to the pinned ESP-Hosted image (built once and cached) so
  # developer builds embed the same C6 update image as releases.
  [string]$C6Firmware,
  # Build without an embedded C6 image; Firmware & Updates then reports
  # the update image as not included.
  [switch]$WithoutC6,
  # Test build with the Stage-1 DSP A/B harness (RTL_DSP AB ...). Never ship.
  [switch]$DspAb,
  # Compile out per-stage DSP timing (profiler observer-effect runs).
  [switch]$NoDspStageTiming,
  # Test build with the Stage-2 frontend benchmark lab (RTL_DSP LAB ...). Never ship.
  [switch]$DspLab,
  # Enables authenticated C6-only power fault injection. Never ship.
  [switch]$C6FaultTest,
  # Logs receive-interface callbacks for the Settings Wi-Fi crash investigation.
  [switch]$WifiRxDiag,
  # Logs the pre-app DMA reservation budget. Test builds only; never ship.
  [switch]$MapStartupDiag,
  # Host artifact only. Inclusion in flash_args does not flash a device.
  [string]$WorldMap,
  [string]$MapCatalogBaseUrl = 'https://github.com/hardcoreerik/OrcMaps/releases/download/maps-catalog-v1',
  [string]$MapCatalogPublicKeyFile = (Join-Path $PSScriptRoot '..\main\maps_production_public.pem')
)

$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
# Resolve the toolchain rather than hardcoding one layout. An ESP-IDF put in
# place by the IDF Installer lives at <root>\<version>\esp-idf with its venv
# under <root>\tools\python\<version>\venv, which fixed paths miss: export.ps1
# then runs activate.py under whatever `python` the PATH happens to offer and
# dies with a NativeCommandError before anything is compiled.
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
  throw "No ESP-IDF 5.5.4 Python environment found. Checked: $($pythonEnvCandidates -join ', ')"
}
$env:IDF_PYTHON_ENV_PATH = $resolvedPythonEnv
$ccache = Get-ChildItem -LiteralPath (Join-Path $env:IDF_TOOLS_PATH 'ccache') `
  -Filter ccache.exe -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
$toolPaths = @($env:IDF_PYTHON_ENV_PATH + '\Scripts')
if ($ccache) { $toolPaths += $ccache.DirectoryName }
$env:PATH = ($toolPaths -join ';') + ";$env:PATH"
$resolvedC6Firmware = $null
if ($WithoutC6 -and $C6Firmware) { throw 'Use either -C6Firmware or -WithoutC6, not both.' }
if (-not $WithoutC6 -and -not $C6Firmware) {
  $C6Firmware = & (Join-Path $PSScriptRoot 'resolve-hosted-c6.ps1') -IdfPath $IdfPath
}
if ($C6Firmware) {
  if (-not (Test-Path -LiteralPath $C6Firmware -PathType Leaf)) {
    throw "C6 firmware does not exist: $C6Firmware"
  }
  $resolvedC6Firmware = (Resolve-Path -LiteralPath $C6Firmware).Path
}
# The installer also writes a profile that exports the same environment without
# re-running activate.py; prefer it, and fall back to export.ps1.
$eimProfile = Join-Path $env:IDF_TOOLS_PATH 'Microsoft.v5.5.4.PowerShell_profile.ps1'
$environmentReady =
  $env:IDF_PATH -eq $IdfPath -and
  $null -ne (Get-Command ninja -ErrorAction SilentlyContinue) -and
  $null -ne (Get-Command riscv32-esp-elf-gcc -ErrorAction SilentlyContinue)
if (-not $environmentReady) {
  if (Test-Path -LiteralPath $eimProfile -PathType Leaf) { . $eimProfile }
  else { . (Join-Path $IdfPath 'export.ps1') }
}
if ($ccache) { $env:PATH = "$($ccache.DirectoryName);$env:PATH" }
$global:LASTEXITCODE = 0
Set-Location (Join-Path $PSScriptRoot '..')
$buildDir = 'build-native-hosted3'

# sdkconfig.defaults is the source; regenerate the per-build Kconfig cache so
# a prior transport choice cannot silently survive a configuration change.
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
Copy-Item -LiteralPath 'sdkconfig.defaults' -Destination (Join-Path $buildDir 'sdkconfig') -Force
$configureArgs = @('-B', $buildDir, '-D', "SDKCONFIG=$buildDir/sdkconfig",
                   '-D', "ORCSDR_WORLD_MAP=$WorldMap",
                   '-D', "ORCSDR_MAP_CATALOG_BASE_URL=$MapCatalogBaseUrl",
                   '-D', "ORCSDR_MAP_CATALOG_PUBLIC_KEY_FILE=$MapCatalogPublicKeyFile",
                   '-D', 'SDKCONFIG_DEFAULTS=sdkconfig.defaults',
                   '-D', "ORCSDR_DSP_AB=$(if ($DspAb) { 1 } else { 0 })",
                   '-D', "ORCSDR_DSP_STAGE_TIMING=$(if ($NoDspStageTiming) { 0 } else { 1 })",
                   '-D', "ORCSDR_DSP_LAB=$(if ($DspLab) { 1 } else { 0 })",
                   '-D', "ORCSDR_C6_FAULT_TEST=$(if ($C6FaultTest) { 1 } else { 0 })",
                   '-D', "ORCSDR_WIFI_RX_DIAG=$(if ($WifiRxDiag) { 1 } else { 0 })",
                   '-D', "ORCSDR_MAP_STARTUP_DIAG=$(if ($MapStartupDiag) { 1 } else { 0 })")
if ($resolvedC6Firmware) {
  $configureArgs += @('-D', "C6_FIRMWARE_BIN=$resolvedC6Firmware")
}
function Update-M5ComponentRegistration {
  # The legacy register_component() macro expands to add_library() with the
  # component's absolute source paths unquoted, so a checkout under a path with
  # a space ("ORC SDR") breaks configure with "Cannot find source file .../ORC".
  # Rewriting the macro call to idf_component_register keeps the same sources
  # and survives a space. Returns how many files it had to change.
  $patched = 0
  foreach ($relativePath in @(
    'managed_components\m5stack__m5gfx\CMakeLists.txt',
    'managed_components\m5stack__m5unified\CMakeLists.txt'
  )) {
    if (-not (Test-Path -LiteralPath $relativePath -PathType Leaf)) { continue }
    $content = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $relativePath))
    $updated = $content -replace '(?m)^\s*register_component\(\)\s*$', @'
idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
    REQUIRES ${COMPONENT_REQUIRES}
    )
'@
    if ($updated -ne $content) {
      [IO.File]::WriteAllText((Resolve-Path -LiteralPath $relativePath), $updated,
                              [Text.UTF8Encoding]::new($false))
      Write-Host "Updated legacy M5 component registration: $relativePath"
      $patched++
    }
  }
  return $patched
}

# Patch before configure, and again after: the component manager re-resolves
# managed_components/ during configure and can restore the legacy macro.
$null = Update-M5ComponentRegistration
idf.py @configureArgs reconfigure
if ($LASTEXITCODE -ne 0) {
  if ((Update-M5ComponentRegistration) -gt 0) { idf.py @configureArgs reconfigure }
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# The component-manager step above fetches (or re-resolves) managed_components/,
# which can overwrite an already-patched M5GFX checkout. Apply the patch after
# reconfigure, not before, so a fresh checkout/worktree has something to patch
# and a stale patch can't be silently dropped by re-resolution.
& (Join-Path $PSScriptRoot 'apply-m5gfx-tab5-pageflip.ps1')
& (Join-Path $PSScriptRoot 'apply-esp-hosted-trampoline-fix.ps1')

$required = @(
  'CONFIG_ESP32P4_TAB5_C6_BOARD=y',
  '# CONFIG_ESP_HOSTED_AUTO_CALL_INIT_BEFORE_APP_MAIN is not set',
  'CONFIG_ESP_HOSTED_HOST_RESET_GPIO=15',
  'CONFIG_ESP_HOSTED_HOST_FEAT_PEER_DATA=y',
  'CONFIG_ESP_HOSTED_HOST_SDIO_PIN_CLK=12',
  'CONFIG_ESP_HOSTED_HOST_SDIO_PIN_CMD=13',
  'CONFIG_ESP_HOSTED_HOST_SDIO_PIN_D0=11',
  'CONFIG_ESP_HOSTED_HOST_SDIO_PIN_D1=10',
  'CONFIG_ESP_HOSTED_HOST_SDIO_PIN_D2=9',
  'CONFIG_ESP_HOSTED_HOST_SDIO_PIN_D3=8',
  'CONFIG_ESP_HOSTED_HOST_CP_RESET_STRATEGY_ONLY_IF_NECESSARY=y',
  'CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384',
  'CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y',
  'CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y',
  'CONFIG_ESP_TASK_WDT_PANIC=y'
)
$config = Get-Content -LiteralPath (Join-Path $buildDir 'sdkconfig')
foreach ($line in $required) {
  if ($config -notcontains $line) { throw "Generated sdkconfig disagrees with defaults: $line" }
}

idf.py -B $buildDir build
if ($LASTEXITCODE -ne 0) { throw "ESP-IDF build failed with exit code $LASTEXITCODE." }

# The CMake ALL target verifies OrcMaps' linked miniz symbols before success.
