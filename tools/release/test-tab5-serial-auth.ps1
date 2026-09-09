$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '..\tab5_serial_auth.ps1')

[byte[]]$sample = 0, 1, 15, 16, 127, 128, 254, 255
$encoded = ConvertTo-OrcHex $sample
if ($encoded -ne '00010f107f80feff') { throw "Unexpected hex encoding: $encoded" }
[byte[]]$decoded = ConvertFrom-OrcHex $encoded
if (-not (Test-OrcFixedTimeEqual $sample $decoded)) { throw 'Hex round trip failed.' }
if (Test-OrcFixedTimeEqual $sample ([byte[]](0, 1, 15, 16, 127, 128, 254, 0))) {
    throw 'Fixed-time comparison accepted different data.'
}
if (Test-OrcFixedTimeEqual $sample ([byte[]](0, 1))) {
    throw 'Fixed-time comparison accepted different lengths.'
}
try {
    [void](ConvertFrom-OrcHex 'not-hex')
    throw 'Invalid hex was accepted.'
} catch {
    if ($_.Exception.Message -eq 'Invalid hex was accepted.') { throw }
}

$toolsRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
foreach ($scriptName in @('copy_to_tab5_sd.ps1', 'copy_from_tab5_sd.ps1',
                           'get_from_tab5_sd.ps1')) {
    $scriptText = [IO.File]::ReadAllText((Join-Path $toolsRoot $scriptName))
    if ($scriptText -notmatch 'Connect-Tab5AuthenticatedSerial') {
        throw "$scriptName does not authenticate its serial session."
    }
    if ($scriptText -notmatch 'PairingKeyPath') {
        throw "$scriptName does not expose the pairing-key path."
    }
}

$firmwarePath = Join-Path $toolsRoot '..\apps\orcsdr-tab5\ui\main.cpp'
$firmwareText = [IO.File]::ReadAllText((Resolve-Path $firmwarePath))
foreach ($marker in @('SD_LIST_ERROR auth_required', 'SD_GET_ERROR auth_required',
                       'sd_get_abort("auth_expired")',
                       'end_authenticated_session("auth_expired")',
                       'RTL_WIFI_SCAN_ERROR auth_required',
                       'RTL_WIFI_RESULTS_ERROR auth_required',
                       'RTL_WIFI_PROFILES_ERROR auth_required',
                       'RTL_ADSB_LOCATION_ERROR auth_required',
                       'RTL_LOCATION_ERROR auth_required')) {
    if (-not $firmwareText.Contains($marker)) {
        throw "Firmware SD authentication guard is missing: $marker"
    }
}

foreach ($scriptPath in @(
    '..\apps\orcsdr-tab5\tools\run-tab5-ui-regression.ps1',
    '..\apps\orcsdr-tab5\tools\test-tab5-wifi-release.ps1')) {
    $resolved = Resolve-Path (Join-Path $toolsRoot $scriptPath)
    $scriptText = [IO.File]::ReadAllText($resolved)
    if ($scriptText -notmatch 'Connect-Tab5AuthenticatedSerial') {
        throw "$resolved does not use the shared serial authentication helper."
    }
}

Write-Host 'TAB5_SERIAL_AUTH_TEST_OK'
