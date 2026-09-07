$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
& wsl.exe --cd $repoRoot bash ./tools/test-pocsag-store.sh
if ($LASTEXITCODE -ne 0) { throw "POCSAG store host tests failed with exit code $LASTEXITCODE." }
Write-Host 'POCSAG store host tests passed (optimized + ASan/LSan/UBSan).'
