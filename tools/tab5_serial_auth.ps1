Set-StrictMode -Version Latest

function ConvertFrom-OrcHex([string]$Hex) {
    if ($Hex -notmatch '^(?:[0-9A-Fa-f]{2})+$') { throw 'Invalid hexadecimal text.' }
    $bytes = New-Object byte[] ($Hex.Length / 2)
    for ($index = 0; $index -lt $bytes.Length; $index++) {
        $bytes[$index] = [Convert]::ToByte($Hex.Substring($index * 2, 2), 16)
    }
    return ,$bytes
}

function ConvertTo-OrcHex([byte[]]$Bytes) {
    return (($Bytes | ForEach-Object { $_.ToString('x2') }) -join '')
}

function Test-OrcFixedTimeEqual([byte[]]$Left, [byte[]]$Right) {
    if ($Left.Length -ne $Right.Length) { return $false }
    [byte]$difference = 0
    for ($index = 0; $index -lt $Left.Length; $index++) {
        $difference = $difference -bor ($Left[$index] -bxor $Right[$index])
    }
    return $difference -eq 0
}

function Connect-Tab5AuthenticatedSerial {
    param(
        [Parameter(Mandatory = $true)] [IO.Ports.SerialPort]$Serial,
        [Parameter(Mandatory = $true)] [string]$PairingKeyPath,
        [Parameter(Mandatory = $true)] [scriptblock]$WaitLine
    )

    $resolvedKey = (Resolve-Path -LiteralPath $PairingKeyPath -ErrorAction Stop).Path
    $keyText = [IO.File]::ReadAllText($resolvedKey).Trim()
    if ($keyText -notmatch '^[0-9A-Fa-f]{64}$') {
        throw 'Pairing key must contain exactly 32 hexadecimal bytes.'
    }
    [byte[]]$key = ConvertFrom-OrcHex $keyText

    $Serial.WriteLine('PAIR ' + $keyText)
    $pair = & $WaitLine -Prefixes @('PAIR_OK', 'PAIR_LOCKED', 'PAIR_INVALID') -TimeoutSeconds 10
    if ($pair -ne 'PAIR_OK') { throw "Device pairing failed: $pair" }

    [byte[]]$nonce = New-Object byte[] 16
    $rng = [Security.Cryptography.RNGCryptoServiceProvider]::new()
    try { $rng.GetBytes($nonce) } finally { $rng.Dispose() }
    $hmac = [Security.Cryptography.HMACSHA256]::new($key)
    try {
        [byte[]]$hostInput = [Text.Encoding]::ASCII.GetBytes('host') + $nonce
        [byte[]]$hostProof = $hmac.ComputeHash($hostInput)
        $Serial.WriteLine('AUTH ' + (ConvertTo-OrcHex $nonce) + ' ' + (ConvertTo-OrcHex $hostProof))
        $reply = & $WaitLine -Prefixes @('AUTH_OK', 'AUTH_DENIED', 'AUTH_ERROR', 'AUTH_INVALID') -TimeoutSeconds 10
        if ($reply -notmatch '^AUTH_OK ([0-9A-Fa-f]{64})$') {
            throw "Device authentication failed: $reply"
        }
        [byte[]]$expected = $hmac.ComputeHash([byte[]]([Text.Encoding]::ASCII.GetBytes('device') + $nonce))
        if (-not (Test-OrcFixedTimeEqual (ConvertFrom-OrcHex $Matches[1]) $expected)) {
            throw 'Device authentication proof did not match.'
        }
    } finally {
        $hmac.Dispose()
    }
}
