#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -d "$root/build" ]; then rm -rf -- "$root/build"; fi
