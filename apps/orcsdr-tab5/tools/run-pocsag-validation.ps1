param(
  [ValidatePattern('^COM[0-9]+$')]
  [string]$Port = 'COM17',
  [string]$ScanListPath,
  [uint32[]]$FrequencyHz = @(454025000, 454225000, 454350000, 929612500, 929662500, 929937500, 152240000),
  [ValidateRange(5, 300)]
  [int]$DwellSeconds = 15,
  [switch]$SkipDiscoveryScan,
  [ValidateRange(30, 1800)]
  [int]$ScanTimeoutSeconds = 90,
  [uint32]$RestoreFrequencyHz = 152007500,
  [string]$PairingKeyPath = (Join-Path $PSScriptRoot '..\..\..\.orclink\ui-doc.key'),
  [string]$LogPath
)

# Survey tool, not a pass/fail regression gate: real pager traffic may or may
# not exist on any given candidate frequency at test time, so "nothing heard"
# is a valid, reportable outcome, not a script failure. It only throws on
# things that indicate the device/serial link itself is broken (crash,
# command rejected, timeout).

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$script:serial = $null
$script:key = $null
$script:lastPing = [DateTime]::MinValue
$script:logFile = $null

if (-not $LogPath) {
  $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
  $logDir = Join-Path $PSScriptRoot '..\..\..\artifacts\pocsag-validation'
  New-Item -ItemType Directory -Path $logDir -Force | Out-Null
  $LogPath = Join-Path $logDir "run-$stamp.log"
}
$script:logFile = $LogPath

function Write-Log([string]$Line) {
  # Write-Host, not Write-Output: this is called from inside helper functions
  # (Read-LineUntil in particular) whose own return value must stay just the
  # matched line -- Write-Output here would leak into and corrupt that return
  # value, since PowerShell functions emit every unswallowed Write-Output
  # call, not only the final `return`.
  $stamped = "$(Get-Date -Format 'HH:mm:ss.fff') $Line"
  Write-Host $stamped
  Add-Content -LiteralPath $script:logFile -Value $stamped
}

function Test-FatalLine([string]$Line) {
  return $Line -match '(?i)Guru Meditation|panic(?:ked|\x27ed)?|assert failed|abort\(|task watchdog|interrupt wdt|brownout detector|ESP-ROM:esp32p4|rst:0x|out of memory|alloc(?:ation)? failed|heap corruption'
}

function Read-LineUntil([scriptblock]$Match, [int]$TimeoutSeconds) {
  $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
  while ([DateTime]::UtcNow -lt $deadline) {
    if (([DateTime]::UtcNow - $script:lastPing).TotalSeconds -ge 2) {
      try { $script:serial.WriteLine('PING') } catch {}
      $script:lastPing = [DateTime]::UtcNow
    }
    try {
      $line = $script:serial.ReadLine().Trim()
      if (!$line) { continue }
      Write-Log "< $line"
      if (Test-FatalLine $line) { throw "Device crash/reset detected: $line" }
      if (& $Match $line) { return $line }
    } catch [System.TimeoutException] {}
  }
  return $null
}

function Get-Field([string]$Line, [string]$Name) {
  if ($Line -match "(?:^| )$([regex]::Escape($Name))=([^ ]+)") { return $Matches[1].Trim('"') }
  throw "Missing $Name in status: $Line"
}

function Get-PocsagStatus {
  $script:serial.WriteLine('RTL_POCSAG STATUS')
  $line = Read-LineUntil { param($v) $v -match '^RTL_POCSAG_STATUS ' } 5
  if ($null -eq $line) { throw 'Timed out waiting for RTL_POCSAG_STATUS.' }
  return [pscustomobject]@{
    Raw          = $line
    Lock         = [int](Get-Field $line 'lock')
    Baud         = [int](Get-Field $line 'baud')
    Inverted     = [int](Get-Field $line 'inverted')
    FrequencyHz  = [uint32](Get-Field $line 'frequency_hz')
    Scanning     = [int](Get-Field $line 'scanning')
    Batches      = [uint32](Get-Field $line 'batches')
    Codewords    = [uint32](Get-Field $line 'codewords')
    Valid        = [uint32](Get-Field $line 'valid')
    Corrected    = [uint32](Get-Field $line 'corrected')
    Uncorrectable = [uint32](Get-Field $line 'uncorrectable')
    Messages     = [uint32](Get-Field $line 'messages')
  }
}

function Read-PairingKey {
  if (Test-Path -LiteralPath $PairingKeyPath -PathType Leaf) {
    $text = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $PairingKeyPath).Path).Trim()
    if ($text -notmatch '^[0-9A-Fa-f]{64}$') {
      throw 'Pairing key file must contain exactly 32 hexadecimal bytes.'
    }
    return ,([Convert]::FromHexString($text))
  }
  Write-Log "PAIR_KEY generating new key at $PairingKeyPath (first pairing on this device)"
  $key = [byte[]]::new(32)
  [Security.Cryptography.RandomNumberGenerator]::Fill($key)
  New-Item -ItemType Directory -Path (Split-Path -Parent $PairingKeyPath) -Force | Out-Null
  [IO.File]::WriteAllText($PairingKeyPath, [Convert]::ToHexString($key))
  return ,$key
}

function Connect-Authenticated {
  if ($null -eq $script:key) { $script:key = Read-PairingKey }
  $script:serial.WriteLine('PAIR ' + [Convert]::ToHexString($script:key))
  $paired = Read-LineUntil { param($v) $v -match '^PAIR_' } 5
  if ($paired -notmatch '^PAIR_OK$') { throw "Pairing failed or key mismatch: $paired" }

  $nonce = [byte[]]::new(16)
  [Security.Cryptography.RandomNumberGenerator]::Fill($nonce)
  $hmac = [Security.Cryptography.HMACSHA256]::new($script:key)
  try {
    $proof = $hmac.ComputeHash([byte[]]([Text.Encoding]::ASCII.GetBytes('host') + $nonce))
    $script:serial.WriteLine('AUTH ' + [Convert]::ToHexString($nonce) + ' ' +
                             [Convert]::ToHexString($proof))
    $reply = Read-LineUntil { param($v) $v -match '^AUTH_' } 5
    if ($reply -notmatch '^AUTH_OK ([0-9A-Fa-f]{64})$') {
      throw "Device authentication failed: $reply"
    }
    $expected = $hmac.ComputeHash([byte[]]([Text.Encoding]::ASCII.GetBytes('device') + $nonce))
    if (-not [Security.Cryptography.CryptographicOperations]::FixedTimeEquals(
        [Convert]::FromHexString($Matches[1]), $expected)) {
      throw 'Device authentication proof did not match.'
    }
  } finally {
    $hmac.Dispose()
  }
  Write-Log 'POCSAG_VALIDATION_AUTH verified=true'
}

function Open-Tab5Port {
  $script:serial = [System.IO.Ports.SerialPort]::new($Port, 115200, 'None', 8, 'One')
  $script:serial.ReadTimeout = 300
  $script:serial.NewLine = "`n"
  $script:serial.DtrEnable = $false
  $script:serial.RtsEnable = $false
  $script:serial.Open()
  Start-Sleep -Milliseconds 500
  $script:serial.DiscardInBuffer()
}

function Push-ScanList {
  Write-Log "SD_PUSH begin path=$ScanListPath"
  $source = (Resolve-Path -LiteralPath $ScanListPath).Path
  $bytes = [IO.File]::ReadAllBytes($source)
  if ($bytes.Length -le 0 -or $bytes.Length -gt 16MB) { throw 'Scan list file must be 1 byte to 16 MiB.' }
  $sha = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
  $destination = '/orcsdr/pocsag_scan.cfg'
  $pathHex = [Convert]::ToHexString([Text.Encoding]::ASCII.GetBytes($destination)).ToLowerInvariant()

  $script:serial.WriteLine('RTL_STOP')
  $stopping = Read-LineUntil { param($v) $v -match '^RTL_STOPPING$' } 8
  if ($null -eq $stopping) { throw 'RTL_STOP was not acknowledged.' }

  # RTL_STOP's own reply is just the immediate "RTL_STOPPING" ack -- the
  # capture task actually finishing (what SD_PUT's radio_busy check cares
  # about) happens asynchronously afterward, so poll SD_PUT_BEGIN itself
  # rather than race a specific follow-up status line.
  $ready = $null
  $deadline = [DateTime]::UtcNow.AddSeconds(10)
  while ([DateTime]::UtcNow -lt $deadline -and $null -eq $ready) {
    $script:serial.WriteLine("SD_PUT_BEGIN $($bytes.Length) $sha $pathHex")
    $reply = Read-LineUntil { param($v) $v -match '^SD_PUT_(READY|ERROR)' } 5
    if ($reply -match '^SD_PUT_READY chunk=(\d+) ') {
      $ready = $reply
    } elseif ($reply -eq 'SD_PUT_ERROR radio_busy') {
      Start-Sleep -Milliseconds 500
    } else {
      throw "SD push rejected: $reply"
    }
  }
  if ($null -eq $ready) { throw 'Radio never went idle for the SD push.' }
  $chunkSize = [int]$Matches[1]

  for ($offset = 0; $offset -lt $bytes.Length; $offset += $chunkSize) {
    $len = [Math]::Min($chunkSize, $bytes.Length - $offset)
    # Protocol is length-prefixed raw bytes, not hex-in-the-command-line:
    # SD_PUT_CHUNK <decimal length> -> device replies SD_PUT_DATA, ready to
    # read exactly that many raw bytes off the wire -- see copy_to_tab5_sd.ps1,
    # the reference implementation this mirrors.
    $script:serial.WriteLine("SD_PUT_CHUNK $len")
    $dataReady = Read-LineUntil { param($v) $v -match '^SD_PUT_(DATA|ERROR)' } 10
    if ($dataReady -notmatch '^SD_PUT_DATA ') { throw "SD push chunk rejected at offset ${offset}: $dataReady" }
    $script:serial.Write($bytes, $offset, $len)
    $ack = Read-LineUntil { param($v) $v -match '^SD_PUT_(ACK|ERROR)' } 10
    if ($ack -notmatch '^SD_PUT_ACK ') { throw "SD push chunk not acknowledged at offset ${offset}: $ack" }
  }
  $done = Read-LineUntil { param($v) $v -match '^SD_PUT_(DONE|ERROR)' } 10
  if ($done -notmatch '^SD_PUT_DONE ') { throw "SD push did not complete: $done" }
  Write-Log "SD_PUSH done bytes=$($bytes.Length) sha256=$sha"

  Write-Log 'REBOOT requesting device restart to reload /orcsdr/pocsag_scan.cfg'
  $script:serial.WriteLine('RTL_RESET')
  [void](Read-LineUntil { param($v) $v -match '^RTL_RESETTING$' } 5)
  $script:serial.Close()
  Start-Sleep -Seconds 2

  $deadline = [DateTime]::UtcNow.AddSeconds(30)
  $reconnected = $false
  while ([DateTime]::UtcNow -lt $deadline -and -not $reconnected) {
    try {
      Open-Tab5Port
      $reconnected = $true
    } catch {
      Start-Sleep -Seconds 1
    }
  }
  if (-not $reconnected) { throw "Could not reopen $Port after reboot." }
  $ready2 = Read-LineUntil { param($v) $v -match '^RTL_POCSAG_SELF_CHECK_OK$|^BOOT_STAGE ready$' } 25
  if ($null -eq $ready2) { throw 'Device did not report a clean boot after reset.' }
  Connect-Authenticated
  Start-Sleep -Seconds 2
  $script:serial.DiscardInBuffer()
  Write-Log 'REBOOT device back online and re-authenticated'
}

# Boot- and reconnect-time telemetry (the orclink JSON hello/snapshot/journal
# stream) can occasionally overlap a command's reply; a single silent
# timeout right after a reconnect isn't necessarily the command being lost,
# so give each RTL_POCSAG_TUNE a couple of retries before treating it as a
# real failure.
function Send-PocsagTune([uint32]$Hz) {
  for ($attempt = 1; $attempt -le 3; $attempt++) {
    $script:serial.WriteLine("RTL_POCSAG_TUNE $Hz")
    $tune = Read-LineUntil { param($v) $v -match '^RTL_POCSAG_TUNE_' } 8
    if ($tune -match '^RTL_POCSAG_TUNE_OK') { return }
    Write-Log "POCSAG_VALIDATION_TUNE_RETRY frequency_hz=$Hz attempt=$attempt reply=`"$tune`""
    Start-Sleep -Seconds 1
  }
  throw "Tune to $Hz failed after 3 attempts."
}

try {
  Open-Tab5Port
  Connect-Authenticated

  if ($ScanListPath) { Push-ScanList }

  $baseline = Get-PocsagStatus
  Write-Log "POCSAG_VALIDATION_BASELINE $($baseline.Raw)"

  $results = @()
  foreach ($hz in $FrequencyHz) {
    Send-PocsagTune $hz
    Write-Log "POCSAG_VALIDATION_TUNE frequency_hz=$hz"

    $bestLock = 0
    $startStatus = Get-PocsagStatus
    $deadline = [DateTime]::UtcNow.AddSeconds($DwellSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
      Start-Sleep -Seconds 3
      $status = Get-PocsagStatus
      if ($status.Lock -gt $bestLock) { $bestLock = $status.Lock }
      Write-Log "POCSAG_VALIDATION_SAMPLE frequency_hz=$hz $($status.Raw)"
    }
    $endStatus = Get-PocsagStatus
    $messagesSeen = $endStatus.Messages - $startStatus.Messages
    $validSeen = $endStatus.Valid - $startStatus.Valid
    $correctedSeen = $endStatus.Corrected - $startStatus.Corrected
    $lockLabel = switch ($bestLock) { 0 { 'no_signal' } 1 { 'searching' } 2 { 'locked' } 3 { 'lost' } default { 'unknown' } }
    $results += [pscustomobject]@{
      FrequencyHz = $hz
      BestLock    = $lockLabel
      ValidCodewords = $validSeen
      CorrectedCodewords = $correctedSeen
      Messages    = $messagesSeen
    }
    Write-Log ("POCSAG_VALIDATION_FREQUENCY_RESULT frequency_hz=$hz best_lock=$lockLabel " +
               "valid_codewords=$validSeen corrected_codewords=$correctedSeen messages=$messagesSeen")
  }

  $scanFound = $false
  $scanDetail = $null
  if (-not $SkipDiscoveryScan) {
    $script:serial.WriteLine('RTL_POCSAG_SCAN')
    $queued = Read-LineUntil { param($v) $v -match '^RTL_POCSAG_SCAN_' } 5
    if ($queued -notmatch '^RTL_POCSAG_SCAN_QUEUED$') { throw "Discovery scan did not start: $queued" }
    Write-Log 'POCSAG_VALIDATION_SCAN started=true'
    $done = Read-LineUntil { param($v) $v -match '^RTL_POCSAG_DISCOVERY_DONE ' } $ScanTimeoutSeconds
    if ($null -eq $done) { throw 'Discovery scan did not report completion in time.' }
    $scanDetail = $done
    $scanFound = $done -match 'found=1'
  }

  Send-PocsagTune $RestoreFrequencyHz

  # @(...): Where-Object returns a bare scalar (no .Count) when exactly one
  # object matches, instead of a one-element array -- wrap to force an array.
  $anyTraffic = @($results | Where-Object { $_.Messages -gt 0 -or $_.ValidCodewords -gt 0 }).Count -gt 0
  Write-Log ("POCSAG_VALIDATION_RESULT result=INFO frequencies_tested=$($results.Count) " +
             "traffic_observed=$([int]$anyTraffic) discovery_scan_found=$([int]$scanFound) " +
             "log=`"$script:logFile`"")
  $results | Format-Table -AutoSize | Out-String | ForEach-Object { Write-Log $_ }
} finally {
  if ($null -ne $script:serial -and $script:serial.IsOpen) {
    try {
      $script:serial.WriteLine('RTL_POCSAG_SCAN_STOP')
      [void](Read-LineUntil { param($v) $v -match '^RTL_POCSAG_SCAN_STOP_' } 3)
      Send-PocsagTune $RestoreFrequencyHz
    } catch {
      Write-Log "POCSAG_VALIDATION_CLEANUP warning=$($_.Exception.Message)"
    }
    $script:serial.Close()
  }
  if ($null -ne $script:key) {
    [Array]::Clear($script:key, 0, $script:key.Length)
  }
}
