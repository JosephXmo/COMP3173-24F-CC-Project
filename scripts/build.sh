#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
configuration=${1:-debug}
case "$configuration" in debug|release) ;; *) echo 'usage: build.sh [debug|release]' >&2; exit 64;; esac
if [ -n "${CC:-}" ]; then compiler=$CC
elif command -v clang >/dev/null 2>&1; then compiler=clang
else compiler=cc
fi
out="$root/build/$configuration"
mkdir -p "$out/obj"
cat > "$out/c23-probe.c" <<'EOF'
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error A conforming C23 language mode is required
#endif
int main(void) { return 0; }
EOF
run() { printf '%s ' "$@"; printf '\n'; "$@"; }
run "$compiler" -std=c23 -c "$out/c23-probe.c" -o "$out/obj/c23-probe.o"
if [ "$configuration" = debug ]; then set -- -O0 -g3; else set -- -O2 -DNDEBUG; fi
# CFLAGS may contain additional development flags; never evaluated as shell code.
for file in "$root"/src/*.c; do
    base=${file##*/}; base=${base%.c}
    run "$compiler" -std=c23 -Wall -Wextra -Wpedantic -Wconversion -Wshadow "$@" ${CFLAGS:-} -I"$root/include" -c "$file" -o "$out/obj/$base.o"
done
set --
for file in "$root"/src/*.c; do base=${file##*/}; set -- "$@" "$out/obj/${base%.c}.o"; done
run "$compiler" "$@" ${CFLAGS:-} ${LDFLAGS:-} -o "$out/uslc"
for file in "$root"/tests/test_*.c; do
    base=${file##*/}; base=${base%.c}
    if [ "$configuration" = debug ]; then set -- -O0 -g3; else set -- -O2 -DNDEBUG; fi
    run "$compiler" -std=c23 -Wall -Wextra -Wpedantic -Wconversion -Wshadow "$@" ${CFLAGS:-} -I"$root/include" -I"$root/tests" -c "$file" -o "$out/obj/$base.o"
done
set --
for file in "$root"/src/*.c "$root"/tests/test_*.c; do
    base=${file##*/}; base=${base%.c}; [ "$base" = main ] || set -- "$@" "$out/obj/$base.o"
done
run "$compiler" "$@" ${CFLAGS:-} ${LDFLAGS:-} -o "$out/uslc_tests"
printf 'Built strict C23 %s with %s\n' "$configuration" "$compiler"
