"""Compare the Python port with a supplied, already-built C reference executable."""

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import random
import subprocess
import sys
import tempfile

from uslc.cli import parse_options
from uslc.compiler import compile_source
from uslc.grammar import core_grammar
from uslc.printer import grammar_dump, slr_dump
from uslc.runtime import default_workers, require_runtime
from uslc.slr import build_slr
from uslc.source import Source


def main() -> int:
    require_runtime()
    if len(sys.argv) != 2:
        print("usage: python -m scripts.differential <C-executable>", file=sys.stderr)
        return 64
    binary = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[1]
    table = build_slr(core_grammar())

    def reference(args, data=None):
        return subprocess.run([str(binary), *args], input=data, capture_output=True, cwd=root, timeout=30)

    def check(condition, message):
        if not condition:
            raise AssertionError(message)

    fixtures = sorted((root / "tests" / "fixtures").glob("*.set"))
    for fixture in fixtures:
        args_path = fixture.with_suffix(".args")
        arguments = args_path.read_text().splitlines() if args_path.exists() else []
        path = "tests/fixtures/" + fixture.name
        options = parse_options(arguments + [path])
        result = compile_source(Source(path, fixture.read_bytes()), table=table, phase=options.phase,
                                artifacts=True, compact=options.compact, trace=True)
        with tempfile.TemporaryDirectory() as directory:
            process = reference([*arguments, "--emit-dir", directory, "--trace-parser", path])
            stdout = process.stdout.replace(b"\r\n", b"\n")
            stderr = process.stderr.replace(b"\r\n", b"\n")
            check(process.returncode == result.status, f"fixture exit: {fixture.name}")
            expected_out = result.report().encode() if result.status == 0 else b""
            expected_err = "".join(result.trace).encode() + (result.report().encode() if result.status else b"")
            check(stdout == expected_out and stderr == expected_err, f"fixture output/trace: {fixture.name}")
            actual = {file.name: file.read_bytes() for file in Path(directory).glob("*.json")}
            check(actual == result.artifacts, f"fixture artifacts: {fixture.name}")

    dump = reference(["--dump-grammar", "--dump-slr"])
    expected = grammar_dump(table.grammar, table.analysis) + slr_dump(table)
    check(dump.returncode == 0 and dump.stderr.replace(b"\r\n", b"\n") == expected.encode(), "grammar/SLR dump")

    rng = random.Random(317324)
    sources = []
    for _ in range(100):
        a, b, c, d = (rng.randrange(10000) for _ in range(4))
        sources.append(f"show ({a}-{b})*{c}+{d}.\n")
    for _ in range(100):
        n, low, high, special = (rng.randrange(30) for _ in range(4))
        sources.append(f"show {n}@({{x:x>{low}}}I{{y:y<{high}}}U{{z:z={special}}}).\n")
    for _ in range(30):
        n, inner, threshold = (rng.randrange(30) for _ in range(3))
        sources.append(f"show {n}@{{x:{inner}@({{y:y>x}}U{{x:x<{threshold}}})}}.\n")
    sources += ["show 00.", "show 9223372036854775808.", "show -1.", "show 1",
                "show missing.", "let int x be x.show x.", "show 1@2.",
                "show 9223372036854775807+1.", "show 1<2|9223372036854775807+1>0.",
                "show {a:2@{b:b>a}}U{b:b<1}."]

    def compare(text):
        source = Source.from_text(text, "-")
        result = compile_source(source, table=table)
        process = reference(["-"], source.data)
        check(result.status == process.returncode, f"status: {text}")
        actual = process.stderr if result.status else process.stdout
        check(actual.replace(b"\r\n", b"\n") == result.report().encode(), f"report: {text}")
        check(not (process.stdout if result.status else process.stderr), f"unexpected stream: {text}")

    with ThreadPoolExecutor(max_workers=default_workers()) as executor:
        list(executor.map(compare, sources))
    print(f"C/Python parity: {len(fixtures)} full-artifact fixtures, grammar/SLR dumps and traces, "
          f"{len(sources)} generated/boundary programs passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
