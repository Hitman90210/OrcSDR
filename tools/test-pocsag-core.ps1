$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
& wsl.exe --cd $repoRoot bash ./tools/test-pocsag-core.sh
if ($LASTEXITCODE -ne 0) { throw "POCSAG core host tests failed with exit code $LASTEXITCODE." }
Write-Host 'POCSAG core host tests passed (optimized + ASan/LSan/UBSan).'
