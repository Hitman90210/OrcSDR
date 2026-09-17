$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$runner = Join-Path $repo 'apps\orcsdr-tab5\tools\run-shortwave-ui-regression.ps1'
if (-not (Test-Path -LiteralPath $runner -PathType Leaf)) {
  throw "Missing Shortwave regression runner: $runner"
}

$output = & $runner -SelfCheck -ResetDevice -PairingKeyPath $PSCommandPath 2>&1
if ($LASTEXITCODE -ne 0) { throw "Runner self-check failed: $output" }
if ($output -notmatch 'SHORTWAVE_UI_RUNNER_SELF_CHECK pass=1') {
  throw "Runner did not report a successful self-check: $output"
}

Write-Host 'test-shortwave-ui-regression-runner: PASS'
