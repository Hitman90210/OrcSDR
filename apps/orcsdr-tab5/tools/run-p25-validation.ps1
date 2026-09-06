param(
  [ValidatePattern('^COM[0-9]+$')]
  [string]$Port = 'COM17',
  [ValidateRange(1000000, 2000000000)]
  [uint32]$ControlFrequencyHz = 453812500,
  [ValidateRange(10, 180)]
  [int]$ControlLockSeconds = 45,
  [ValidateRange(30, 3600)]
  [int]$CallWindowSeconds = 300,
  [ValidateRange(2, 8)]
  [int]$MinimumGrantCount = 2,
  [ValidateRange(65536, 16777216)]
  [uint32]$MinimumHeapBytes = 262144,
  [ValidateRange(256, 8192)]
  [uint32]$MinimumVoiceStackHeadroom = 1024,
  [string]$PairingKeyPath = (Join-Path $PSScriptRoot '..\..\..\.orclink\ui-doc.key'),
  [string]$BackupPath,
  [switch]$CaptureFixture
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$script:serial = $null
$script:key = $null
$script:lastPing = [DateTime]::MinValue
$script:tempNvs = $null

function Test-FatalLine([string]$Line) {
  return $Line -match '(?i)Guru Meditation|panic(?:ked|\x27ed)?|assert failed|abort\(|task watchdog|interrupt wdt|brownout detector|ESP-ROM:esp32p4|rst:0x|out of memory|alloc(?:ation)? failed|heap corruption'
}

function Read-LineUntil([scriptblock]$Match, [int]$TimeoutSeconds) {
  $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
  while ([DateTime]::UtcNow -lt $deadline) {
    if (([DateTime]::UtcNow - $script:lastPing).TotalSeconds -ge 2) {
      $script:serial.WriteLine('PING')
      $script:lastPing = [DateTime]::UtcNow
    }
    try {
      $line = $script:serial.ReadLine().Trim()
      if (!$line) { continue }
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

function Get-Status {
  $script:serial.WriteLine('RTL_P25_STATUS')
  $line = Read-LineUntil { param($value) $value -match '^RTL_P25_STATUS ' } 5
  if ($null -eq $line) { throw 'Timed out waiting for RTL_P25_STATUS.' }
  Write-Host $line
  return [pscustomobject]@{
    Raw = $line
    FrequencyHz = [uint32](Get-Field $line 'frequency_hz')
    Survey = [int](Get-Field $line 'survey')
    FrameSync = [int](Get-Field $line 'frame_sync')
    Identity = [int](Get-Field $line 'identity')
    TsbkGood = [uint32](Get-Field $line 'tsbk_good')
    Grants = [uint32](Get-Field $line 'grants')
    GrantEvents = [uint32](Get-Field $line 'grant_events')
    ControlHz = [uint32](Get-Field $line 'control_hz')
    Follow = Get-Field $line 'follow'
    ImbeFrames = [uint32](Get-Field $line 'imbe_frames')
    PcmFrames = [uint32](Get-Field $line 'pcm_frames')
    VoiceQueueDrops = [uint32](Get-Field $line 'voice_queue_drops')
    HeapFree = [uint32](Get-Field $line 'heap_free')
    HeapMin = [uint32](Get-Field $line 'heap_min')
    VoiceStack = [uint32](Get-Field $line 'voice_stack_hwm')
    UsbOverruns = [uint32](Get-Field $line 'usb_overruns')
    UsbDrops = [uint32](Get-Field $line 'usb_drops')
    IqDrops = [uint32](Get-Field $line 'iq_drops')
    AudioDrops = [uint32](Get-Field $line 'audio_drops')
  }
}

function Read-PairingKey {
  if ($PairingKeyPath -and (Test-Path -LiteralPath $PairingKeyPath -PathType Leaf)) {
    $text = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $PairingKeyPath).Path).Trim()
    if ($text -notmatch '^[0-9A-Fa-f]{64}$') {
      throw 'Pairing key file must contain exactly 32 hexadecimal bytes.'
    }
    return ,([Convert]::FromHexString($text))
  }
  if (!$BackupPath -or -not (Test-Path -LiteralPath $BackupPath -PathType Leaf)) {
    throw 'Provide a valid -PairingKeyPath or -BackupPath.'
  }

  $python = 'C:\Espressif\python_env\idf5.5_py3.14_env\Scripts\python.exe'
  $nvsTool = 'C:\Espressif\frameworks\esp-idf-v5.5.4\components\nvs_flash\nvs_partition_tool\nvs_tool.py'
  if (-not (Test-Path -LiteralPath $python -PathType Leaf) -or
      -not (Test-Path -LiteralPath $nvsTool -PathType Leaf)) {
    throw 'ESP-IDF 5.5.4 NVS tools are not installed.'
  }
  $script:tempNvs = [IO.Path]::Combine([IO.Path]::GetTempPath(),
    'orcsdr-p25-nvs-' + [guid]::NewGuid().ToString('N') + '.bin')
  $backup = [IO.File]::OpenRead((Resolve-Path -LiteralPath $BackupPath).Path)
  try {
    if ($backup.Length -lt 0xF000) { throw 'Backup is too small to contain OrcSDR NVS.' }
    $backup.Position = 0x9000
    $buffer = [byte[]]::new(0x6000)
    if ($backup.Read($buffer, 0, $buffer.Length) -ne $buffer.Length) {
      throw 'Could not read the complete NVS partition from the backup.'
    }
    [IO.File]::WriteAllBytes($script:tempNvs, $buffer)
  } finally {
    $backup.Dispose()
  }
  $entry = (& $python $nvsTool -d minimal -f json $script:tempNvs 2>$null | ConvertFrom-Json) |
    Where-Object { $_.namespace -eq 'orclink' -and $_.key -eq 'pair_key' } |
    Select-Object -First 1
  if ($null -eq $entry) { throw 'The backup has no OrcSDR pairing key.' }
  $key = [Convert]::FromBase64String($entry.data)
  if ($key.Length -ne 32) { throw 'The backup pairing key is invalid.' }
  return ,$key
}

function Connect-Authenticated {
  $script:key = Read-PairingKey
  $nonce = [byte[]]::new(16)
  [Security.Cryptography.RandomNumberGenerator]::Fill($nonce)
  $hmac = [Security.Cryptography.HMACSHA256]::new($script:key)
  try {
    $proof = $hmac.ComputeHash([byte[]]([Text.Encoding]::ASCII.GetBytes('host') + $nonce))
    $script:serial.WriteLine('AUTH ' + [Convert]::ToHexString($nonce) + ' ' +
                             [Convert]::ToHexString($proof))
    $reply = Read-LineUntil { param($line) $line -match '^AUTH_' } 5
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
  Write-Output 'P25_VALIDATION_AUTH verified=true'
}

try {
  $script:serial = [System.IO.Ports.SerialPort]::new($Port, 115200, 'None', 8, 'One')
  $script:serial.ReadTimeout = 200
  $script:serial.NewLine = "`n"
  $script:serial.DtrEnable = $false
  $script:serial.RtsEnable = $false
  $script:serial.Open()
  Start-Sleep -Milliseconds 500
  $script:serial.DiscardInBuffer()
  Connect-Authenticated

  $script:serial.WriteLine("RTL_TUNE P25 $ControlFrequencyHz")
  $tune = Read-LineUntil { param($line) $line -match '^RTL_TUNE_' } 8
  if ($tune -notmatch '^RTL_TUNE_OK') { throw "P25 tune failed: $tune" }
  Write-Output $tune

  $baseline = $null
  $lockDeadline = [DateTime]::UtcNow.AddSeconds($ControlLockSeconds)
  while ([DateTime]::UtcNow -lt $lockDeadline -and $null -eq $baseline) {
    $status = Get-Status
    if ($status.Survey -eq 0 -and $status.Follow -eq 'control' -and
        $status.FrameSync -eq 1 -and $status.Identity -eq 1 -and
        $status.TsbkGood -gt 0 -and $status.FrequencyHz -eq $status.ControlHz) {
      $baseline = $status
    } else {
      Start-Sleep -Seconds 2
    }
  }
  if ($null -eq $baseline) { throw 'P25 control lock and identity were not established.' }
  $activeControlHz = $baseline.ControlHz
  Write-Output "P25_VALIDATION_CONTROL locked=true requested_hz=$ControlFrequencyHz active_hz=$activeControlHz"

  $fixturePath = $null
  if ($CaptureFixture) {
    $started = $null
    $captureDeadline = [DateTime]::UtcNow.AddSeconds($ControlLockSeconds)
    while ([DateTime]::UtcNow -lt $captureDeadline -and $null -eq $started) {
      $script:serial.WriteLine('RTL_P25_IQ_START')
      $response = Read-LineUntil {
        param($line)
        $line -match '^RTL_IQ_START source=p25 ' -or
          $line -match '^RTL_P25_IQ_ERROR '
      } 5
      if ($response -match '^RTL_IQ_START source=p25 ') {
        $started = $response
      } elseif ($response -notmatch '^RTL_P25_IQ_ERROR control_channel_required$') {
        throw "P25 IQ capture could not start: $response"
      } else {
        Start-Sleep -Seconds 1
      }
    }
    if ($null -eq $started) { throw 'P25 IQ capture did not start.' }
    $ready = Read-LineUntil { param($line) $line -match '^RTL_IQ_DONE storage=psram source=p25 ' } 5
    if ($null -eq $ready) { throw 'P25 IQ capture did not fill its bounded buffer.' }
    $script:serial.WriteLine('RTL_P25_IQ_STOP')
    $saved = Read-LineUntil { param($line) $line -match '^RTL_IQ_DONE path=' } 10
    if ($saved -notmatch 'path="([^"]+)" source=p25 bytes=([0-9]+)') {
      throw "P25 IQ capture was not saved: $saved"
    }
    $fixturePath = $Matches[1]
    if ([uint32]$Matches[2] + 36 -gt 1048576) {
      throw 'P25 fixture exceeded the 1 MiB file limit.'
    }
    Write-Output "P25_VALIDATION_CAPTURE path=`"$fixturePath`" bytes=$($Matches[2])"
  }

  $maxGrants = $baseline.GrantEvents
  $maxImbe = $baseline.ImbeFrames
  $maxPcm = $baseline.PcmFrames
  $minHeap = [Math]::Min($baseline.HeapFree, $baseline.HeapMin)
  $minStack = if ($baseline.VoiceStack -gt 0) { $baseline.VoiceStack } else { [uint32]::MaxValue }
  $voiceSeen = $false
  $returnSeen = $false
  $relockSeen = $false
  $deadline = [DateTime]::UtcNow.AddSeconds($CallWindowSeconds)
  Write-Output "P25_VALIDATION_SOAK started=true seconds=$CallWindowSeconds"
  while ([DateTime]::UtcNow -lt $deadline -and -not $relockSeen) {
    $slice = [DateTime]::UtcNow.AddSeconds(3)
    while ([DateTime]::UtcNow -lt $slice) {
      $line = Read-LineUntil { param($value) $value -match '^RTL_P25_FOLLOW_(VOICE|RETURN) ' } 1
      if ($null -eq $line) { continue }
      Write-Output $line
      if ($line -match '^RTL_P25_FOLLOW_VOICE ') { $voiceSeen = $true }
      if ($line -match '^RTL_P25_FOLLOW_RETURN ') { $returnSeen = $true }
    }
    $status = Get-Status
    $maxGrants = [Math]::Max($maxGrants, $status.GrantEvents)
    $maxImbe = [Math]::Max($maxImbe, $status.ImbeFrames)
    $maxPcm = [Math]::Max($maxPcm, $status.PcmFrames)
    $minHeap = [Math]::Min($minHeap, [Math]::Min($status.HeapFree, $status.HeapMin))
    if ($status.VoiceStack -gt 0) { $minStack = [Math]::Min($minStack, $status.VoiceStack) }
    if ($status.UsbOverruns -gt $baseline.UsbOverruns -or
        $status.UsbDrops -gt $baseline.UsbDrops -or
        $status.IqDrops -gt $baseline.IqDrops -or
        $status.AudioDrops -gt $baseline.AudioDrops -or
        $status.VoiceQueueDrops -gt $baseline.VoiceQueueDrops) {
      throw "P25 drop counters grew during validation: $($status.Raw)"
    }
    $relockSeen = $returnSeen -and $status.Follow -eq 'control' -and
                  $status.FrameSync -eq 1 -and $status.TsbkGood -gt $baseline.TsbkGood
  }

  if ($maxGrants - $baseline.GrantEvents -lt $MinimumGrantCount) {
    throw "Only $($maxGrants - $baseline.GrantEvents) voice grant events were observed."
  }
  if (-not $voiceSeen) { throw 'No P25 voice retune was observed.' }
  if ($maxImbe -le $baseline.ImbeFrames) { throw 'IMBE frame count did not grow.' }
  if ($maxPcm -le $baseline.PcmFrames) { throw 'PCM sample count did not grow.' }
  if (-not $returnSeen -or -not $relockSeen) { throw 'Control-channel return and relock were not observed.' }
  if ($minHeap -lt $MinimumHeapBytes) { throw "Heap floor failed: $minHeap bytes." }
  if ($minStack -eq [uint32]::MaxValue -or $minStack -lt $MinimumVoiceStackHeadroom) {
    throw "P25 voice task stack headroom failed: $minStack."
  }

  if ($fixturePath) {
    $script:serial.WriteLine('RTL_STOP')
    $stopped = Read-LineUntil { param($line) $line -match '^RTL_STOP_RESULT ' } 8
    if ($stopped -notmatch '^RTL_STOP_RESULT ESP_OK$') {
      throw "Could not stop radio for replay: $stopped"
    }
    $script:serial.WriteLine("RTL_P25_REPLAY $fixturePath")
    $replay = Read-LineUntil { param($line) $line -match '^RTL_P25_REPLAY_(DONE|ERROR) ' } 30
    if ($replay -notmatch '^RTL_P25_REPLAY_DONE .* nid_good=([1-9][0-9]*) .* tsbk_good=([1-9][0-9]*) ') {
      throw "P25 fixture replay failed: $replay"
    }
    Write-Output $replay
  }

  Write-Output (
    "P25_VALIDATION_RESULT result=PASS control_hz=$activeControlHz " +
    "grant_events=$($maxGrants - $baseline.GrantEvents) " +
    "imbe_frames=$maxImbe pcm_frames=$maxPcm min_heap=$minHeap " +
    "voice_stack_hwm=$minStack voice_return=$([int]$returnSeen) relock=$([int]$relockSeen)")
} finally {
  if ($null -ne $script:serial -and $script:serial.IsOpen) { $script:serial.Close() }
  if ($null -ne $script:key) {
    [Security.Cryptography.CryptographicOperations]::ZeroMemory($script:key)
  }
  if ($script:tempNvs -and [IO.File]::Exists($script:tempNvs)) {
    [IO.File]::Delete($script:tempNvs)
  }
}
