$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
& wsl.exe --cd $repoRoot bash ./tools/test-p25-core.sh
if ($LASTEXITCODE -ne 0) { throw "P25 core host tests failed with exit code $LASTEXITCODE." }
Write-Host 'P25 core host tests passed (optimized + ASan/LSan/UBSan).'
