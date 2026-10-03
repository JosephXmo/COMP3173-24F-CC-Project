"""Compile multiple files concurrently and emit reports in input order."""

from pathlib import Path
import sys

from uslc.compiler import compile_many
from uslc.runtime import default_workers, require_runtime
from uslc.source import Source


def main() -> int:
    require_runtime()
    args = sys.argv[1:]
    workers = default_workers()
    if args[:1] == ["--jobs"]:
        if len(args) < 3 or not args[1].isascii() or not args[1].isdecimal() or not 1 <= int(args[1]) <= 32:
            print("usage: python -m scripts.batch [--jobs 1..32] <source-file>...", file=sys.stderr)
            return 64
        workers = int(args[1])
        args = args[2:]
    if not args:
        print("usage: python -m scripts.batch [--jobs 1..32] <source-file>...", file=sys.stderr)
        return 64
    try:
        sources = [Source(path, Path(path).read_bytes()) for path in args]
    except OSError as error:
        print(f"uslc: cannot read batch source: {error}", file=sys.stderr)
        return 66
    results = compile_many(sources, workers=workers)
    status = 0
    for result in results:
        stream = sys.stderr if result.status else sys.stdout
        stream.write(result.report())
        stream.flush()
        if not status:
            status = result.status
    return status


if __name__ == "__main__":
    raise SystemExit(main())
