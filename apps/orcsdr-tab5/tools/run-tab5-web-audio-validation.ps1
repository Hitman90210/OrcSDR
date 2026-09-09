param(
  [ValidatePattern('^COM[0-9]+$')]
  [string]$Port = 'COM3',
  [ValidateRange(1000000, 2000000000)]
  [uint32]$FrequencyHz = 96100000,
  [string]$PairingKeyPath = (Join-Path $PSScriptRoot '..\..\..\.orclink\ui-doc.key')
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\..\tools\tab5_serial_auth.ps1')
$serial = [IO.Ports.SerialPort]::new($Port, 921600, 'None', 8, 'One')
$serial.NewLine = "`n"
$serial.ReadTimeout = 250

function Wait-Line([string]$Pattern, [int]$Seconds = 15) {
  $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
  $keepalive = [DateTime]::UtcNow.AddSeconds(1)
  while ([DateTime]::UtcNow -lt $deadline) {
    if ([DateTime]::UtcNow -ge $keepalive) {
      $serial.WriteLine('PING')
      $keepalive = [DateTime]::UtcNow.AddSeconds(1)
    }
    try { $line = $serial.ReadLine().Trim() } catch [TimeoutException] { continue }
    if (!$line) { continue }
    Write-Host $line
    if ($line -match '(?i)Guru Meditation|panic(?:ked)?|assert failed|task watchdog|brownout') {
      throw "Device fault detected: $line"
    }
    if ($line -match $Pattern) { return $line }
  }
  throw "Timed out waiting for: $Pattern"
}

function Send-Wait([string]$Command, [string]$Pattern, [int]$Seconds = 15) {
  $serial.WriteLine($Command)
  return Wait-Line $Pattern $Seconds
}

function Wait-AuthLine([string[]]$Prefixes, [int]$TimeoutSeconds = 10) {
  $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
  while ([DateTime]::UtcNow -lt $deadline) {
    try { $line = $serial.ReadLine().Trim() } catch [TimeoutException] { continue }
    if (!$line) { continue }
    Write-Host $line
    foreach ($prefix in $Prefixes) {
      if ($line.StartsWith($prefix, [StringComparison]::Ordinal)) { return $line }
    }
  }
  throw "Timed out waiting for: $($Prefixes -join ', ')"
}

function Get-WavStats([byte[]]$Bytes) {
  if ($Bytes.Length -lt 48 -or [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne 'RIFF' -or
      [Text.Encoding]::ASCII.GetString($Bytes, 8, 4) -ne 'WAVE') {
    throw 'Web audio response is not a valid WAV file.'
  }
  $dataBytes = [BitConverter]::ToUInt32($Bytes, 40)
  if ($dataBytes -gt $Bytes.Length - 44 -or ($dataBytes % 2) -ne 0) {
    throw 'Web audio WAV has an invalid data length.'
  }
  $count = [int]($dataBytes / 2)
  [double]$squares = 0
  [int]$nonzero = 0
  for ($i = 0; $i -lt $count; $i++) {
    $sample = [BitConverter]::ToInt16($Bytes, 44 + $i * 2)
    if ($sample -ne 0) { $nonzero++ }
    $squares += [double]$sample * $sample
  }
  $rms = if ($count) { [Math]::Sqrt($squares / $count) } else { 0 }
  return [pscustomobject]@{ Samples = $count; Nonzero = $nonzero; Rms = $rms }
}

$initial = $null
$initialSound = $true
$initialWeb = $false
$initialWifi = $false
try {
  $serial.Open()
  $serial.DiscardInBuffer()
  $wait = { param($Prefixes, $TimeoutSeconds) Wait-AuthLine $Prefixes $TimeoutSeconds }
  Connect-Tab5AuthenticatedSerial -Serial $serial -PairingKeyPath $PairingKeyPath -WaitLine $wait

  $initial = Send-Wait 'RTL_UI STATUS' '^RTL_UI_STATUS '
  $sound = Send-Wait 'RTL_SOUND' '^RTL_SOUND_STATUS '
  $initialSound = $sound -match 'enabled=1'
  $web = Send-Wait 'RTL_WEB_STATUS' '^RTL_WEB_STATUS '
  $initialWeb = $web -match 'enabled=1'
  $wifi = Send-Wait 'RTL_WIFI_STATUS' '^RTL_WIFI_STATUS '
  $initialWifi = $wifi -match 'connected=1'

  [void](Send-Wait "RTL_TUNE FM $FrequencyHz" "^RTL_TUNE_OK band=FM frequency_hz=$FrequencyHz$")
  [void](Send-Wait 'RTL_SOUND OFF' '^RTL_SOUND_OK enabled=0$')
  if (!$initialWifi) {
    [void](Send-Wait 'RTL_WIFI_CONNECT_SAVED LIVE' '^RTL_WIFI_CONNECT_QUEUED saved_profile=0 mode=live$')
    [void](Wait-Line '^RTL_WIFI_CONNECTED$' 45)
  }
  [void](Send-Wait 'RTL_WEB ON' '^RTL_WEB_OK enabled=1 ')

  $url = $null
  for ($attempt = 0; $attempt -lt 12 -and !$url; $attempt++) {
    Start-Sleep -Milliseconds 500
    $status = Send-Wait 'RTL_WEB_STATUS' '^RTL_WEB_STATUS '
    if ($status -match 'listening=1 url=(http://[^ ]+/)') { $url = $Matches[1] }
  }
  if (!$url) { throw 'Web console did not begin listening.' }

  $client = [Net.WebClient]::new()
  try {
    $stats = 1..3 | ForEach-Object {
      Get-WavStats ($client.DownloadData("$($url)api/audio.wav?t=$([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds())"))
    }
  } finally { $client.Dispose() }
  $samples = ($stats | Measure-Object Samples -Sum).Sum
  $nonzero = ($stats | Measure-Object Nonzero -Sum).Sum
  $peakRms = ($stats | Measure-Object Rms -Maximum).Maximum
  if ($samples -lt 4800 -or $nonzero -eq 0 -or $peakRms -lt 1) {
    throw "Muted web audio contained no live signal: samples=$samples nonzero=$nonzero rms=$peakRms"
  }
  Write-Output ("RTL_WEB_AUDIO_VALIDATION_RESULT pass=1 local_sound=0 clips=3 samples={0} nonzero={1} peak_rms={2:N1} url={3}" -f
                $samples, $nonzero, $peakRms, $url)
} finally {
  if ($serial.IsOpen) {
    try {
      if (!$initialWeb) { [void](Send-Wait 'RTL_WEB OFF' '^RTL_WEB_OK enabled=0 ') }
      if ($initialSound) { [void](Send-Wait 'RTL_SOUND ON' '^RTL_SOUND_OK enabled=1$') }
      if ($initial -match 'band=([A-Z]+) frequency_hz=([0-9]+)') {
        [void](Send-Wait "RTL_TUNE $($Matches[1]) $($Matches[2])" '^RTL_TUNE_(?:OK|UNAVAILABLE) ')
      }
      if (!$initialWifi) { [void](Send-Wait 'RTL_WIFI_DISCONNECT' '^RTL_WIFI_DISCONNECT_OK$' 30) }
    } catch { Write-Warning "State restoration failed: $_" }
    $serial.Close()
  }
}
