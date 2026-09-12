from concurrent.futures import ThreadPoolExecutor
import sys
import threading
import unittest

from uslc.compiler import compile_many, compile_source
from uslc.runtime import require_runtime, runtime_info
from uslc.source import Source
from .common import TABLE


class ParallelTests(unittest.TestCase):
    def test_requested_runtime(self):
        require_runtime()
        self.assertEqual(sys.version_info[:3], (3, 14, 5))
        self.assertTrue(runtime_info()["free_threaded"])
        self.assertFalse(sys._is_gil_enabled())

    def test_batch_order_and_artifact_identity(self):
        sources = [Source.from_text(f"let int x be {i}.show x@{{y:y>4}}.", f"case-{i}") for i in range(24)]
        sources[7] = Source.from_text("show missing.", "case-7")
        serial = compile_many(sources, workers=1, artifacts=True)
        parallel = compile_many(sources, workers=4, artifacts=True)
        for index, (left, right) in enumerate(zip(serial, parallel)):
            self.assertEqual(right.source.name, f"case-{index}")
            self.assertEqual(left.report(), right.report())
            self.assertEqual(left.artifacts, right.artifacts)
            self.assertEqual(left.status, right.status)
        self.assertEqual(parallel[7].status, 3)
        self.assertEqual(parallel[8].status, 0)

    def test_simultaneous_compilations_share_only_immutable_table(self):
        barrier = threading.Barrier(4)

        def worker(index):
            barrier.wait(timeout=10)
            results = []
            for i in range(20):
                source = Source.from_text(f"let int x be {index * 100 + i}.show {{x:x>1}}U{{y:y>2}}.")
                results.append(compile_source(source, table=TABLE, artifacts=True, compact=True))
            return results

        with ThreadPoolExecutor(max_workers=4) as executor:
            batches = list(executor.map(worker, range(4)))
        for batch in batches:
            for result in batch:
                self.assertEqual(result.status, 0, result.report())
                self.assertEqual(result.display, "{ x : ((x > 1) | (x > 2)) }")
                self.assertEqual(result.value.binding, 3)
        self.assertFalse(sys._is_gil_enabled())

    def test_batch_lex_errors_do_not_cancel_siblings(self):
        sources = [Source.from_text("show 1."), Source.from_text("show 00."), Source.from_text("show 2.")]
        results = compile_many(sources, workers=4, phase="lex")
        self.assertEqual([result.status for result in results], [0, 1, 0])

    def test_empty_batch_and_invalid_workers(self):
        self.assertEqual(compile_many([], workers=4), [])
        with self.assertRaises(ValueError):
            compile_many([], workers=0)
