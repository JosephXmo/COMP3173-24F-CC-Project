#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
configuration=${1:-debug}
case "$configuration" in debug|release) ;; *) echo 'Usage: test.sh [debug|release]' >&2;exit 64;; esac
if [ "$#" -gt 1 ]; then echo 'Too many arguments' >&2;exit 64;fi
sh "$root/scripts/build.sh" "$configuration"
cd "$root"
CC=${CC:-clang};PKG_CONFIG=${PKG_CONFIG:-pkg-config}
json_cflags=$("$PKG_CONFIG" --cflags json-c);json_libs=$("$PKG_CONFIG" --libs json-c)
case "$configuration" in debug) mode='-O0 -g3';; release) mode='-O2 -DNDEBUG';; esac
sanitizers=
if [ "${SANITIZE:-0}" = 1 ]; then sanitizers='-fsanitize=address,undefined -fno-omit-frame-pointer';fi
set --
for object in "build/$configuration/"*.o; do
    case "$object" in */main.o|*/test_*|*/parse_context.o|*/serialization.o|*/serialization_test.o) continue;; esac
    set -- "$@" "$object"
done
"$CC" -std=c23 -D_POSIX_C_SOURCE=200809L -DUSLC_TESTING $mode $sanitizers -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
    $json_cflags -Iinclude -Ibuild/generated -Itests tests/test_*.c src/serialization.c src/parse_context.c "$@" $json_libs -o "build/$configuration/uslc_tests"
"build/$configuration/uslc_tests"
test -s build/generated/parser.output
grep 'State 0' build/generated/parser.output >/dev/null
if grep -i 'conflicts:' build/generated/parser.output; then echo 'Unexpected grammar conflicts' >&2;exit 1;fi
sh scripts/test-cli.sh "$configuration"
