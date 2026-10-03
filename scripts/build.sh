#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
configuration=${1:-debug}
case "$configuration" in debug|release) ;; *) echo 'Usage: build.sh [debug|release]' >&2;exit 64;; esac
if [ "$#" -gt 1 ]; then echo 'Too many arguments' >&2;exit 64;fi
CC=${CC:-clang}; FLEX=${FLEX:-flex}; BISON=${BISON:-bison}; PKG_CONFIG=${PKG_CONFIG:-pkg-config}
export CC FLEX BISON PKG_CONFIG
sh "$root/scripts/check-tools.sh"
cd "$root"
mkdir -p build/generated "build/$configuration"
"$BISON" -Werror=conflicts-sr -Werror=conflicts-rr --defines=build/generated/parser.h \
    --output=build/generated/parser.c --report=state,solved --report-file=build/generated/parser.output src/parser.y
"$FLEX" --header-file=build/generated/lexer.h --outfile=build/generated/lexer.c src/lexer.l
json_cflags=$("$PKG_CONFIG" --cflags json-c)
json_libs=$("$PKG_CONFIG" --libs json-c)
case "$configuration" in debug) mode='-O0 -g3';; release) mode='-O2 -DNDEBUG';; esac
sanitizers=
if [ "${SANITIZE:-0}" = 1 ]; then sanitizers='-fsanitize=address,undefined -fno-omit-frame-pointer';fi
base="-std=c23 -D_POSIX_C_SOURCE=200809L $mode $sanitizers"
for source in src/*.c; do
    name=$(basename "$source" .c)
    "$CC" $base -Wall -Wextra -Wpedantic -Wconversion -Wshadow $json_cflags \
        -Iinclude -Ibuild/generated -c "$source" -o "build/$configuration/$name.o"
done
for name in parser lexer; do
    "$CC" $base -Wall -Wno-unused-function -Wno-unused-but-set-variable $json_cflags -Iinclude -Ibuild/generated \
        -c "build/generated/$name.c" -o "build/$configuration/$name.o"
done
set --
for object in "build/$configuration/"*.o; do
    case "$object" in */test_*|*/serialization_test.o) continue;; esac
    set -- "$@" "$object"
done
"$CC" $base "$@" $json_libs -o "build/$configuration/uslc"
printf 'Built %s/build/%s/uslc\n' "$root" "$configuration"
