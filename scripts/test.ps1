param([ValidateRange(1,32)][int]$Jobs = 4)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    & uv --cache-dir (Join-Path $root '.uv-cache') run --frozen --offline python -m scripts.test --jobs $Jobs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally { Pop-Location }
