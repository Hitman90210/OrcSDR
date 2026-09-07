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
  [switch]$CaptureFixture,
  [switch]$RequireEncryptedVoice,
  [ValidateSet('AUTO', 'C4FM', 'CQPSK')]
  [string]$Modulation = 'AUTO',
  [uint16[]]$WatchTalkgroup = @(),
  [switch]$UseExistingSession,
  [string]$ReplayPath,
  [switch]$ReplayOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$script:serial = $null
$script:key = $null
$script:lastPing = [DateTime]::MinValue
$script:tempNvs = $null
$script:initialSoundEnabled = $null
$script:initialModulation = $null
$script:initialPhase2Trace = $null

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
    EncryptedMutedFrames = [uint32](Get-Field $line 'encrypted_muted_frames')
    EncryptedReturns = [uint32](Get-Field $line 'encrypted_returns')
    HeapFree = [uint32](Get-Field $line 'heap_free')
    HeapMin = [uint32](Get-Field $line 'heap_min')
    VoiceStack = [uint32](Get-Field $line 'voice_stack_hwm')
    UsbOverruns = [uint32](Get-Field $line 'usb_overruns')
    UsbDrops = [uint32](Get-Field $line 'usb_drops')
    IqDrops = [uint32](Get-Field $line 'iq_drops')
    AudioDrops = [uint32](Get-Field $line 'audio_drops')
    ModulationConfigured = Get-Field $line 'modulation_configured'
    ModulationSelected = Get-Field $line 'modulation_selected'
    Phase2Grants = [uint32](Get-Field $line 'p2_grants')
    Phase2SyncWords = [uint32](Get-Field $line 'p2_sync_words')
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

function Test-FrequencyNear([uint32]$Actual, [uint32]$Expected) {
  return [Math]::Abs([int64]$Actual - [int64]$Expected) -le 1000
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

  $phase2Required = $WatchTalkgroup.Count -gt 0
  if ($phase2Required) {
    Write-Output "P25_VALIDATION_PHASE2 watchlist=$($WatchTalkgroup -join ',') acceptance=any_live_phase2_tgid"
    $script:serial.WriteLine('RTL_P25_PHASE2_TRACE')
    $trace = Read-LineUntil { param($line) $line -match '^RTL_P25_PHASE2_TRACE enabled=[01]$' } 5
    if ($trace -notmatch 'enabled=([01])$') { throw 'Could not read Phase II trace state.' }
    $script:initialPhase2Trace = [int]$Matches[1]
    if ($script:initialPhase2Trace -eq 0) {
      $script:serial.WriteLine('RTL_P25_PHASE2_TRACE ON')
      $enabled = Read-LineUntil { param($line) $line -match '^RTL_P25_PHASE2_TRACE_(OK|ERROR) ' } 5
      if ($enabled -notmatch '^RTL_P25_PHASE2_TRACE_OK enabled=1$') {
        throw "Could not enable Phase II trace: $enabled"
      }
    }
  }

  if ($ReplayOnly -and -not $ReplayPath) { throw '-ReplayOnly requires -ReplayPath.' }
  $script:serial.WriteLine('RTL_P25_MODULATION')
  $modeStatus = Read-LineUntil { param($line) $line -match '^RTL_P25_MODULATION ' } 5
  if ($modeStatus -notmatch ' configured=(auto|c4fm|cqpsk) ') {
    throw "Could not read the initial P25 modulation: $modeStatus"
  }
  $script:initialModulation = $Matches[1].ToUpperInvariant()
  $script:serial.WriteLine("RTL_P25_MODULATION $Modulation")
  $modeSet = Read-LineUntil { param($line) $line -match '^RTL_P25_MODULATION_(OK|ERROR) ' } 8
  if ($modeSet -notmatch "^RTL_P25_MODULATION_OK configured=$($Modulation.ToLowerInvariant())$") {
    throw "Could not select P25 modulation: $modeSet"
  }

  $script:serial.WriteLine('RTL_SOUND')
  $sound = Read-LineUntil { param($line) $line -match '^RTL_SOUND_STATUS enabled=[01]$' } 5
  if ($sound -notmatch 'enabled=([01])$') { throw 'Could not read the initial sound state.' }
  $script:initialSoundEnabled = [int]$Matches[1]
  if ($script:initialSoundEnabled -eq 0) {
    $script:serial.WriteLine('RTL_SOUND ON')
    $enabled = Read-LineUntil { param($line) $line -match '^RTL_SOUND_OK enabled=1$' } 5
    if ($null -eq $enabled) { throw 'Could not enable audio for P25 validation.' }
  }

  if ($ReplayOnly -and $UseExistingSession) {
    throw '-UseExistingSession cannot be combined with -ReplayOnly.'
  }
  if (-not $UseExistingSession) {
    $script:serial.WriteLine('RTL_STOP')
    $stopped = Read-LineUntil { param($line) $line -match '^RTL_STOP_RESULT ' } 8
    if ($stopped -notmatch '^RTL_STOP_RESULT ESP_OK$') {
      throw "Could not stop the existing radio session: $stopped"
    }
  }

  if ($ReplayOnly) {
    $script:serial.WriteLine("RTL_P25_REPLAY $ReplayPath")
    $replay = Read-LineUntil { param($line) $line -match '^RTL_P25_REPLAY_(DONE|ERROR) ' } 30
    if ($replay -notmatch '^RTL_P25_REPLAY_DONE .* nid_good=([1-9][0-9]*) .* tsbk_good=([1-9][0-9]*) ') {
      throw "P25 fixture replay failed: $replay"
    }
    $expected = $Modulation.ToLowerInvariant()
    if ($Modulation -ne 'AUTO' -and $replay -notmatch " modulation_selected=$expected(?: |$)") {
      throw "P25 fixture replay selected the wrong demodulator: $replay"
    }
    Write-Output $replay
    Write-Output "P25_VALIDATION_REPLAY result=PASS modulation=$expected path=`"$ReplayPath`""
    return
  }

  if (-not $UseExistingSession) {
    $script:serial.WriteLine("RTL_TUNE P25 $ControlFrequencyHz")
    $tune = Read-LineUntil { param($line) $line -match '^RTL_TUNE_' } 8
    if ($tune -notmatch '^RTL_TUNE_OK') { throw "P25 tune failed: $tune" }
    Write-Output $tune
    Start-Sleep -Seconds 2
  } else {
    Write-Output "P25_VALIDATION_SESSION reuse=true requested_hz=$ControlFrequencyHz"
  }

  $baseline = $null
  $lockDeadline = [DateTime]::UtcNow.AddSeconds($ControlLockSeconds)
  while ([DateTime]::UtcNow -lt $lockDeadline -and $null -eq $baseline) {
    $status = Get-Status
    if ($status.Survey -eq 0 -and $status.Follow -eq 'control' -and
        $status.FrameSync -eq 1 -and $status.Identity -eq 1 -and
        $status.TsbkGood -gt 0 -and
        (Test-FrequencyNear $status.FrequencyHz $ControlFrequencyHz) -and
        (Test-FrequencyNear $status.ControlHz $ControlFrequencyHz)) {
      $baseline = $status
    } else {
      Start-Sleep -Seconds 2
    }
  }
  if ($null -eq $baseline) { throw 'P25 control lock and identity were not established.' }
  $expectedModulation = $Modulation.ToLowerInvariant()
  if ($Modulation -ne 'AUTO' -and $baseline.ModulationSelected -ne $expectedModulation) {
    throw "P25 selected $($baseline.ModulationSelected), expected $expectedModulation."
  }
  $activeControlHz = $baseline.ControlHz
  Write-Output "P25_VALIDATION_CONTROL locked=true requested_hz=$ControlFrequencyHz active_hz=$activeControlHz"

  $fixturePath = $ReplayPath
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
  $maxEncryptedMuted = $baseline.EncryptedMutedFrames
  $maxEncryptedReturns = $baseline.EncryptedReturns
  $minHeap = [Math]::Min($baseline.HeapFree, $baseline.HeapMin)
  $minStack = if ($baseline.VoiceStack -gt 0) { $baseline.VoiceStack } else { [uint32]::MaxValue }
  $voiceSeen = $false
  $returnSeen = $false
  $relockSeen = $false
  $encryptedSeen = $false
  $phase2GrantSeen = $false
  $phase2AcquireSeen = $false
  $phase2SyncSeen = $false
  [uint16]$phase2Tgid = 0
  $deadline = [DateTime]::UtcNow.AddSeconds($CallWindowSeconds)
  Write-Output "P25_VALIDATION_SOAK started=true seconds=$CallWindowSeconds"
  while ([DateTime]::UtcNow -lt $deadline) {
    $slice = [DateTime]::UtcNow.AddSeconds(3)
    while ([DateTime]::UtcNow -lt $slice) {
      $line = Read-LineUntil {
        param($value)
        $value -match '^RTL_P25_FOLLOW_(VOICE|RETURN) ' -or
          $value -match '^RTL_P25_ENCRYPTED ' -or
          $value -match '^P25P2_CALL '
      } 1
      if ($null -eq $line) { continue }
      Write-Output $line
      if ($line -match '^RTL_P25_FOLLOW_VOICE ') { $voiceSeen = $true }
      if ($line -match '^RTL_P25_FOLLOW_RETURN ') { $returnSeen = $true }
      if ($line -match '^RTL_P25_ENCRYPTED ') { $encryptedSeen = $true }
      if ($phase2Required -and $line -match '^P25P2_CALL .*?(?:^| )tg=([0-9]+)(?: |$)') {
        $eventTgid = [uint16]$Matches[1]
        if ($line -match ' event=grant ' -and $phase2Tgid -eq 0) {
          $phase2Tgid = $eventTgid
          $phase2GrantSeen = $true
          Write-Output "P25_VALIDATION_PHASE2_SELECTED tg=$phase2Tgid watchlist_match=$([int]($WatchTalkgroup -contains $phase2Tgid))"
        }
        if ($eventTgid -eq $phase2Tgid -and $line -match ' event=acquisition ') {
          $phase2AcquireSeen = $true
          $voiceSeen = $true
        }
        if ($eventTgid -eq $phase2Tgid -and
            $line -match ' event=completion .* duid_valid=1 .* result=burst_complete') {
          $phase2SyncSeen = $true
          $returnSeen = $true
        }
      }
    }
    $status = Get-Status
    if (-not (Test-FrequencyNear $status.ControlHz $ControlFrequencyHz)) {
      throw "P25 control channel changed during validation: requested=$ControlFrequencyHz active=$($status.ControlHz)."
    }
    $maxGrants = [Math]::Max($maxGrants, $status.GrantEvents)
    $maxImbe = [Math]::Max($maxImbe, $status.ImbeFrames)
    $maxPcm = [Math]::Max($maxPcm, $status.PcmFrames)
    $maxEncryptedMuted = [Math]::Max($maxEncryptedMuted, $status.EncryptedMutedFrames)
    $maxEncryptedReturns = [Math]::Max($maxEncryptedReturns, $status.EncryptedReturns)
    $minHeap = [Math]::Min($minHeap, [Math]::Min($status.HeapFree, $status.HeapMin))
    if ($status.VoiceStack -gt 0) { $minStack = [Math]::Min($minStack, $status.VoiceStack) }
    if ($status.UsbOverruns -gt $baseline.UsbOverruns -or
        $status.UsbDrops -gt $baseline.UsbDrops -or
        $status.IqDrops -gt $baseline.IqDrops -or
        $status.AudioDrops -gt $baseline.AudioDrops -or
        $status.VoiceQueueDrops -gt $baseline.VoiceQueueDrops) {
      throw "P25 drop counters grew during validation: $($status.Raw)"
    }
    $relockSeen = $relockSeen -or ($returnSeen -and $status.Follow -eq 'control' -and
                  $status.FrameSync -eq 1 -and $status.TsbkGood -gt $baseline.TsbkGood)
  }

  if ($phase2Required) {
    if (-not $phase2GrantSeen) { throw 'No live Phase II grant was observed.' }
    if (-not $phase2AcquireSeen) { throw "No Phase II traffic probe started for observed TGID $phase2Tgid." }
    if (-not $phase2SyncSeen) { throw "No complete Phase II burst was captured for observed TGID $phase2Tgid." }
  } elseif ($maxGrants - $baseline.GrantEvents -lt $MinimumGrantCount) {
    throw "Only $($maxGrants - $baseline.GrantEvents) voice grant events were observed."
  }
  if (-not $voiceSeen) { throw 'No P25 voice retune was observed.' }
  if (-not $phase2Required -and $maxImbe -le $baseline.ImbeFrames) {
    throw 'IMBE frame count did not grow.'
  }
  if (-not $phase2Required -and $maxPcm -le $baseline.PcmFrames) {
    throw 'PCM sample count did not grow.'
  }
  if (-not $returnSeen -or -not $relockSeen) { throw 'Control-channel return and relock were not observed.' }
  if ($minHeap -lt $MinimumHeapBytes) { throw "Heap floor failed: $minHeap bytes." }
  if (-not $phase2Required -and
      ($minStack -eq [uint32]::MaxValue -or $minStack -lt $MinimumVoiceStackHeadroom)) {
    throw "P25 voice task stack headroom failed: $minStack."
  }
  if ($RequireEncryptedVoice -and (-not $encryptedSeen -or
      $maxEncryptedMuted -le $baseline.EncryptedMutedFrames -or
      $maxEncryptedReturns -le $baseline.EncryptedReturns)) {
    throw 'No encrypted LDU2 mute and control-channel return were observed.'
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

  $reportedStack = if ($minStack -eq [uint32]::MaxValue) { 0 } else { $minStack }
  Write-Output (
    "P25_VALIDATION_RESULT result=PASS control_hz=$activeControlHz " +
    "grant_events=$($maxGrants - $baseline.GrantEvents) " +
    "imbe_frames=$maxImbe pcm_frames=$maxPcm min_heap=$minHeap " +
    "voice_stack_hwm=$reportedStack voice_return=$([int]$returnSeen) relock=$([int]$relockSeen) " +
    "encrypted_seen=$([int]$encryptedSeen) encrypted_muted_frames=$maxEncryptedMuted " +
    "encrypted_returns=$maxEncryptedReturns phase2_tgid=$phase2Tgid " +
    "phase2_grant=$([int]$phase2GrantSeen) phase2_sync=$([int]$phase2SyncSeen)")
} finally {
  if ($null -ne $script:serial -and $script:serial.IsOpen) {
    if ($script:initialPhase2Trace -eq 0) {
      try {
        $script:serial.WriteLine('RTL_P25_PHASE2_TRACE OFF')
        [void](Read-LineUntil { param($line) $line -match '^RTL_P25_PHASE2_TRACE_OK enabled=0$' } 5)
      } catch {
        Write-Warning "Could not restore Phase II trace state: $($_.Exception.Message)"
      }
    }
    if ($script:initialModulation -and $script:initialModulation -ne $Modulation) {
      try {
        $script:serial.WriteLine("RTL_P25_MODULATION $($script:initialModulation)")
        [void](Read-LineUntil { param($line) $line -match '^RTL_P25_MODULATION_OK ' } 8)
      } catch {
        Write-Warning "Could not restore the initial P25 modulation: $($_.Exception.Message)"
      }
    }
    if ($script:initialSoundEnabled -eq 0) {
      try {
        $script:serial.WriteLine('RTL_SOUND OFF')
        [void](Read-LineUntil { param($line) $line -match '^RTL_SOUND_OK enabled=0$' } 5)
      } catch {
        Write-Warning "Could not restore the initial sound state: $($_.Exception.Message)"
      }
    }
    $script:serial.Close()
  }
  if ($null -ne $script:key) {
    [Security.Cryptography.CryptographicOperations]::ZeroMemory($script:key)
  }
  if ($script:tempNvs -and [IO.File]::Exists($script:tempNvs)) {
    [IO.File]::Delete($script:tempNvs)
  }
}
