param(
  [string]$IdfPath = 'C:\Espressif\v5.5.4\esp-idf',
  [string]$C6Firmware
)

$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
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
$resolvedC6Firmware = $null
if ($C6Firmware) {
  if (-not (Test-Path -LiteralPath $C6Firmware -PathType Leaf)) {
    throw "C6 firmware does not exist: $C6Firmware"
  }
  $resolvedC6Firmware = (Resolve-Path -LiteralPath $C6Firmware).Path
}
$eimProfile = Join-Path $env:IDF_TOOLS_PATH 'Microsoft.v5.5.4.PowerShell_profile.ps1'
$environmentReady =
  $env:IDF_PATH -eq $IdfPath -and
  $null -ne (Get-Command ninja -ErrorAction SilentlyContinue) -and
  $null -ne (Get-Command cmake -ErrorAction SilentlyContinue) -and
  $null -ne (Get-Command riscv32-esp-elf-gcc -ErrorAction SilentlyContinue)
if (-not $environmentReady) {
  if (Test-Path -LiteralPath $eimProfile -PathType Leaf) {
    . $eimProfile
  } else {
    . (Join-Path $IdfPath 'export.ps1')
  }
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
                   '-D', 'SDKCONFIG_DEFAULTS=sdkconfig.defaults')
if ($resolvedC6Firmware) {
  $configureArgs += @('-D', "C6_FIRMWARE_BIN=$resolvedC6Firmware")
}

function Update-M5ComponentRegistration {
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
      [IO.File]::WriteAllText(
        (Resolve-Path -LiteralPath $relativePath),
        $updated,
        [Text.UTF8Encoding]::new($false)
      )
      Write-Host "Updated legacy M5 component registration: $relativePath"
      $patched++
    }
  }
  return $patched
}

# The legacy M5 component macro breaks absolute source paths containing spaces.
# Patch managed copies before configure and retry once if the component manager
# downloaded fresh copies during the first configure attempt.
$null = Update-M5ComponentRegistration
idf.py @configureArgs reconfigure
if ($LASTEXITCODE -ne 0) {
  $patchedAfterConfigure = Update-M5ComponentRegistration
  if ($patchedAfterConfigure -gt 0) {
    idf.py @configureArgs reconfigure
  }
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
