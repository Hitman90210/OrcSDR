$ErrorActionPreference = 'Stop'
$appRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (& git -C $appRoot rev-parse --show-toplevel).Trim()
$appRelative = (& git -C $appRoot rev-parse --show-prefix).Trim().TrimEnd('/')

# Applied in order: the lifecycle patch rewrites how detached and joinable
# tasks are created and reaped, and the handle patch then hardens the pointer
# that create() returns. Keeping them as two files means a build tree that
# already carries the first one is still recognised as patched.
$patches = @(
  @{ File = 'esp-hosted-task-lifecycle.patch';      Name = 'task-lifecycle' },
  @{ File = 'esp-hosted-detached-task-handle.patch'; Name = 'detached-task-handle' }
)

foreach ($entry in $patches) {
  $patch = Join-Path $PSScriptRoot (Join-Path 'patches' $entry.File)
  if (-not (Test-Path -LiteralPath $patch)) { throw "Missing ESP-Hosted patch: $patch" }

  $priorErrorActionPreference = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  & git -C $repoRoot apply --check --ignore-space-change --directory=$appRelative -- $patch 2>$null
  $applyCheckExitCode = $LASTEXITCODE
  $ErrorActionPreference = $priorErrorActionPreference
  if ($applyCheckExitCode -eq 0) {
    & git -C $repoRoot apply --ignore-space-change --directory=$appRelative -- $patch
    if ($LASTEXITCODE -ne 0) { throw "Unable to apply the ESP-Hosted $($entry.Name) patch." }
    continue
  }

  $ErrorActionPreference = 'Continue'
  & git -C $repoRoot apply --reverse --check --ignore-space-change --directory=$appRelative -- $patch 2>$null
  $reverseCheckExitCode = $LASTEXITCODE
  $ErrorActionPreference = $priorErrorActionPreference
  if ($reverseCheckExitCode -ne 0) {
    throw "The installed ESP-Hosted component does not match the $($entry.Name) patch."
  }
}
