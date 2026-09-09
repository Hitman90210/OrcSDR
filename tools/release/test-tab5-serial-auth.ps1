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

Write-Host 'TAB5_SERIAL_AUTH_TEST_OK'
