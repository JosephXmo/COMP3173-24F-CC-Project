#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
configuration=${1:-debug}
case "$configuration" in debug|release) ;; *) echo 'usage: test.sh [debug|release] [--no-build]' >&2; exit 64;; esac
if [ "${2:-}" != --no-build ]; then sh "$root/scripts/build.sh" "$configuration"; fi
cd "$root"
binary="$root/build/$configuration/uslc"
tests="$root/build/$configuration/uslc_tests"
"$tests"
count=0
for source in tests/fixtures/*.set; do
    base=${source%.set}; name=${base##*/}; output="build/$configuration/integration-linux/$name"
    mkdir -p "$output/artifacts"
    set --
    if [ -f "$base.args" ]; then while IFS= read -r argument; do set -- "$@" "$argument"; done < "$base.args"; fi
    if [ -d "$base.artifacts" ]; then set -- "$@" --emit-dir "$output/artifacts"; fi
    status=0
    "$binary" "$@" "$source" > "$output/stdout" 2> "$output/stderr" || status=$?
    expected=$(cat "$base.exit")
    if [ "$status" != "$expected" ]; then echo "$name: exit $status, expected $expected" >&2; exit 1; fi
    "$tests" --compare-text "$base.stdout" "$output/stdout"
    "$tests" --compare-text "$base.stderr" "$output/stderr"
    if [ -d "$base.artifacts" ]; then
        for file in "$base.artifacts"/*.json; do "$tests" --compare "$file" "$output/artifacts/${file##*/}"; done
        for file in "$output/artifacts"/*.json; do [ -f "$base.artifacts/${file##*/}" ] || { echo "unexpected artifact: $file" >&2; exit 1; }; done
    fi
    count=$((count+1))
done
printf '%s integration fixtures passed\n' "$count"
mkdir -p "build/$configuration/cli-linux"
cli="build/$configuration/cli-linux"
check_exit() {
    wanted=$1; shift; actual=0
    "$binary" "$@" > "$cli/stdout" 2> "$cli/stderr" || actual=$?
    [ "$wanted" = "$actual" ] || { echo "CLI: expected exit $wanted, got $actual" >&2; exit 1; }
}
check_exit 64 --phase invalid examples/arithmetic.set
check_exit 64 --unknown
check_exit 64
check_exit 66 tests/fixtures/does-not-exist.set
check_exit 74 --emit-dir "$cli/does-not-exist/nested" examples/arithmetic.set
check_exit 0 --help
check_exit 0 --version
check_exit 0 --dump-grammar --dump-slr
check_exit 0 --trace-parser examples/arithmetic.set
mkdir -p "$cli/reused"
printf 'preserve\n' > "$cli/reused/unrelated.txt"
check_exit 0 --emit-dir "$cli/reused" tests/fixtures/compact.set
check_exit 1 --emit-dir "$cli/reused" tests/fixtures/lex-error.set
set -- "$cli/reused"/*.json
[ "$#" = 1 ] && [ "${1##*/}" = diagnostics.json ] && [ -f "$cli/reused/unrelated.txt" ] || {
    echo 'Stale artifact cleanup failed' >&2; exit 1;
}
printf 'show 3.\n' | "$binary" - > "$cli/stdin.stdout"
"$tests" --compare-text tests/fixtures/stdin.stdout "$cli/stdin.stdout"
printf 'CLI checks passed\n'
