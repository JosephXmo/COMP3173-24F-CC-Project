#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
exec uv --cache-dir "$root/.uv-cache" run --frozen --offline python -m scripts.test --jobs "${1:-4}"
