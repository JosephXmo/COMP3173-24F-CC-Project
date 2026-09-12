#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 0 ]; then echo 'Usage: clean.sh' >&2;exit 64;fi
case "$root" in ''|/) echo 'Refusing unsafe build path' >&2;exit 70;; esac
if [ -L "$root/build" ]; then echo 'Refusing symlinked build directory' >&2;exit 70;fi
rm -rf -- "$root/build"
