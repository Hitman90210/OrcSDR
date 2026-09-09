$ErrorActionPreference = 'Stop'
$appRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$patch = Join-Path $PSScriptRoot 'patches\esp-hosted-sdio-rx-len-cap.patch'
$target = Join-Path $appRoot 'managed_components\espressif__esp_hosted\host\mcu\eh_host_mcu_transport\src\eh_host_bus_sdio.c'
$repoRoot = (& git -C $appRoot rev-parse --show-toplevel).Trim()
$appRelative = (& git -C $appRoot rev-parse --show-prefix).Trim().TrimEnd('/')

if (Test-Path -LiteralPath $target) {
  $text = Get-Content -LiteralPath $target -Raw
  if ($text -match 'ORCSDR-TAB5 #66: streaming/SW_AGGR used to skip the RX length cap') {
    Write-Host 'ESP-Hosted SDIO RX length-cap patch already present.'
    exit 0
  }
}

$prior = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& git -C $repoRoot apply --check --ignore-space-change --directory=$appRelative -- $patch 2>$null
$check = $LASTEXITCODE
$ErrorActionPreference = $prior
if ($check -eq 0) {
  & git -C $repoRoot apply --ignore-space-change --directory=$appRelative -- $patch
  if ($LASTEXITCODE -ne 0) { throw 'Unable to apply ESP-Hosted SDIO RX length-cap patch.' }
  exit 0
}
$ErrorActionPreference = 'Continue'
& git -C $repoRoot apply --reverse --check --ignore-space-change --directory=$appRelative -- $patch 2>$null
$rev = $LASTEXITCODE
$ErrorActionPreference = $prior
if ($rev -ne 0) {
  throw 'The installed ESP-Hosted component does not match the SDIO RX length-cap patch.'
}
