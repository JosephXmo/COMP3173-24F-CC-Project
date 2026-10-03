"""Run independent unittest cases concurrently, and print results in stable order."""

from concurrent.futures import ThreadPoolExecutor
import io
from pathlib import Path
import sys
import unittest

from uslc.runtime import default_workers, require_runtime


def flatten(suite):
    for item in suite:
        if isinstance(item, unittest.TestSuite):
            yield from flatten(item)
        else:
            yield item


def main() -> int:
    require_runtime()
    workers = default_workers()
    arguments = sys.argv[1:]
    if arguments:
        if (len(arguments) != 2 or arguments[0] != "--jobs" or not arguments[1].isascii()
                or not arguments[1].isdecimal() or not 1 <= int(arguments[1]) <= 32):
            print("usage: python -m scripts.test [--jobs 1..32]", file=sys.stderr)
            return 64
        workers = int(arguments[1])
    root = Path(__file__).resolve().parents[1]
    suite = unittest.defaultTestLoader.discover(str(root / "tests"), top_level_dir=str(root))
    tests = sorted(flatten(suite), key=lambda case: case.id())

    def run(case):
        output = io.StringIO()
        result = unittest.TextTestRunner(stream=output, verbosity=0).run(case)
        return case.id(), result, output.getvalue()

    if workers == 1:
        results = list(map(run, tests))
    else:
        with ThreadPoolExecutor(max_workers=workers) as executor:
            results = list(executor.map(run, tests))
    failures = 0
    for name, result, output in results:
        if not result.wasSuccessful():
            failures += len(result.failures) + len(result.errors) + len(result.unexpectedSuccesses)
            print(name)
            print(output, end="")
    print(f"{len(tests)} tests, {failures} failures ({workers} workers; GIL enabled: {sys._is_gil_enabled()})")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
