"""Measure actual workloads without asserting that more threads must be faster."""

from pathlib import Path
import time

from uslc.compiler import compile_many
from uslc.grammar import core_grammar
from uslc.json_writer import encode_json
from uslc.runtime import require_runtime, runtime_info
from uslc.slr import build_slr
from uslc.source import Source


def measure(operation, repeats=3):
    samples = []
    for _ in range(repeats):
        start = time.perf_counter()
        operation()
        samples.append(time.perf_counter() - start)
    return min(samples)


def main() -> int:
    require_runtime()
    root = Path(__file__).resolve().parents[1]
    sources = [Source.from_text("show " + "+".join(str(i % 100) for i in range(160)) + ".", f"bench-{j}")
               for j in range(160)]
    rows = []
    for workers in (1, 2, 4):
        def batch():
            results = compile_many(sources, workers=workers)
            if any(result.status or result.value.integer != 6720 for result in results):
                raise AssertionError("benchmark workload produced an incorrect result")
        rows.append({"workers": workers,
                     "slr_seconds": measure(lambda: build_slr(core_grammar(), workers)),
                     "batch_seconds": measure(batch)})
    document = {"runtime": runtime_info(), "measurement": "minimum of 3 runs",
                "sources": len(sources), "terms_per_source": 160, "results": rows}
    directory = root / "build"
    directory.mkdir(exist_ok=True)
    (directory / "benchmark.json").write_bytes(encode_json(document))
    for row in rows:
        print(f"{row['workers']} workers: SLR {row['slr_seconds']:.6f}s; batch {row['batch_seconds']:.6f}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
