#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
configuration=${1:-debug}
case "$configuration" in debug|release) ;; *) exit 64;; esac
bin="$root/build/$configuration/uslc"
tests="$root/build/$configuration/uslc_tests"
out="$root/build/$configuration/cli tests"
mkdir -p "$out/artifacts" "$out/reference"
for fixture in tests/fixtures/*.set; do
    stem=${fixture%.set}
    status=0
    "$bin" "$fixture" > "$out/stdout" 2> "$out/stderr" || status=$?
    test "$status" -eq "$(cat "$stem.exit")"
    cmp "$stem.stdout" "$out/stdout"
    cmp "$stem.stderr" "$out/stderr"
done
printf 'show 3.\n' > "$out/source with spaces.set"
"$bin" --emit-dir "$out/artifacts" "$out/source with spaces.set" > "$out/stdout"
for name in tokens parser_out ast typing_out evaluation_out diagnostics; do test -s "$out/artifacts/$name.json";done
"$tests" --compare-json "$out/artifacts/evaluation_out.json" tests/fixtures/integer.evaluation.json
cp "$out/artifacts/evaluation_out.json" "$out/reference/evaluation_out.json"
cp "$out/artifacts/parser_out.json" "$out/reference/parser_out.json"
"$bin" --emit-dir "$out/artifacts" "$out/source with spaces.set" > "$out/repeat"
cmp "$out/stdout" "$out/repeat"
cmp "$out/reference/evaluation_out.json" "$out/artifacts/evaluation_out.json"
cmp "$out/reference/parser_out.json" "$out/artifacts/parser_out.json"
"$bin" --compact-json --emit-dir "$out/artifacts" "$out/source with spaces.set" > "$out/stdout"
"$tests" --compare-json "$out/artifacts/evaluation_out.json" "$out/reference/evaluation_out.json"
printf 'preserve\n' > "$out/artifacts/unrelated.txt"
"$bin" --phase parse --emit-dir "$out/artifacts" "$out/source with spaces.set" > "$out/stdout"
test -s "$out/artifacts/ast.json"
test ! -e "$out/artifacts/typing_out.json"
test ! -e "$out/artifacts/evaluation_out.json"
"$bin" --phase lex --emit-dir "$out/artifacts" "$out/source with spaces.set" > "$out/stdout"
test ! -e "$out/artifacts/parser_out.json"
test -s "$out/artifacts/unrelated.txt"
check_error() {
    expected=$1;shift
    status=0
    "$bin" "$@" > "$out/stdout" 2> "$out/stderr" || status=$?
    test "$status" -eq "$expected"
    test ! -s "$out/stdout"
}
check_error 64 --phase invalid -
check_error 64 --emit-dir
check_error 64 --unknown
check_error 64 "$out/source with spaces.set" extra
check_error 66 "$out/missing.set"
check_error 74 --emit-dir "$out/missing-dir" "$out/source with spaces.set"
printf 'show 01.\n' > "$out/bad.set"
check_error 1 --emit-dir "$out/artifacts" "$out/bad.set"
test -s "$out/artifacts/diagnostics.json"
test ! -e "$out/artifacts/tokens.json"
printf 'show 1\n' > "$out/bad.set"
check_error 2 --emit-dir "$out/artifacts" "$out/bad.set"
test -s "$out/artifacts/tokens.json"
test ! -e "$out/artifacts/parser_out.json"
printf 'let set x be 4. show x.\n' > "$out/bad.set"
check_error 3 --emit-dir "$out/artifacts" "$out/bad.set"
test -s "$out/artifacts/ast.json"
test ! -e "$out/artifacts/typing_out.json"
printf 'show 9223372036854775807+1.\n' > "$out/bad.set"
check_error 4 --emit-dir "$out/artifacts" "$out/bad.set"
test -s "$out/artifacts/typing_out.json"
test ! -e "$out/artifacts/evaluation_out.json"
printf 'show 3.\n' | "$bin" - > "$out/stdout"
grep 'value: 3' "$out/stdout" >/dev/null
"$bin" --trace-parser "$out/source with spaces.set" > "$out/stdout" 2> "$out/trace"
grep 'Shifting token' "$out/trace" >/dev/null
grep 'Reducing stack by rule' "$out/trace" >/dev/null
"$bin" --phase check "$out/source with spaces.set" > "$out/stdout"
grep 'typing:   ok' "$out/stdout" >/dev/null
if grep 'evaluate:' "$out/stdout";then exit 1;fi
"$bin" --version > "$out/version"
"$bin" --help > "$out/help"
cp "$out/source with spaces.set" "$out/-leading.set"
(cd "$out" && "$bin" -- -leading.set > stdout)
if [ -e /dev/full ]; then
    status=0
    "$bin" "$out/source with spaces.set" > /dev/full 2> "$out/stderr" || status=$?
    test "$status" -eq 74
fi
printf 'CLI integration tests passed\n'
