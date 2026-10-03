import json
from pathlib import Path
import tempfile
import unittest

from uslc.cli import parse_options, write_artifact
from uslc.compiler import compile_source
from uslc.diagnostic import Diagnostic
from uslc.json_writer import encode_json
from uslc.printer import grammar_dump, slr_dump
from uslc.source import Source
from .common import FIXTURES, TABLE, cli, lf


class OutputTests(unittest.TestCase):
    def test_all_c_golden_fixtures(self):
        artifact_count = 0
        for path in sorted(FIXTURES.glob("*.set")):
            with self.subTest(fixture=path.stem):
                args_path = path.with_suffix(".args")
                options = parse_options(args_path.read_text().splitlines() + [str(path)]
                                        if args_path.exists() else [str(path)])
                source = Source("tests/fixtures/" + path.name, path.read_bytes())
                expected_dir = path.with_suffix(".artifacts")
                result = compile_source(source, table=TABLE, phase=options.phase,
                                        artifacts=expected_dir.exists(), compact=options.compact)
                self.assertEqual(result.status, int(path.with_suffix(".exit").read_text()))
                output = result.report().encode()
                self.assertEqual(output if result.status == 0 else b"", path.with_suffix(".stdout").read_bytes())
                self.assertEqual(output if result.status else b"", path.with_suffix(".stderr").read_bytes())
                if expected_dir.exists():
                    expected = {file.name: file.read_bytes() for file in expected_dir.glob("*.json")}
                    self.assertEqual(result.artifacts, expected)
                    artifact_count += len(expected)
        self.assertEqual(artifact_count, 35)

    def test_json_encoding(self):
        data = {"text": '"\\\b\f\n\r\t\x00\x01\u00e9', "values": [-(1 << 63), True, None, {}]}
        pretty = encode_json(data)
        self.assertEqual(json.loads(pretty), data)
        self.assertTrue(pretty.endswith(b"\n"))
        self.assertFalse(pretty.startswith(b"\xef\xbb\xbf"))
        self.assertIn(b"\\u0000\\u0001\xc3\xa9", pretty)
        self.assertEqual(encode_json(["a", "b"], compact=True), b'["a","b"]\n')
        with self.assertRaises(ValueError):
            encode_json(float("nan"))

    def test_ast_phase_snapshot(self):
        result = compile_source(Source.from_text("show 3."), table=TABLE, artifacts=True)
        before = json.loads(result.artifacts["ast.json"])["root"]
        after = json.loads(result.artifacts["typing_out.json"])["root"]
        self.assertEqual(before["children"][0]["children"][0]["type"], "void")
        self.assertEqual(after["children"][0]["children"][0]["type"], "integer")

    def test_structured_set_result(self):
        result = compile_source(Source.from_text("show {x:x>3}U{y:y<0}."), table=TABLE, artifacts=True)
        value = json.loads(result.artifacts["evaluation_out.json"])["result"]["value"]
        self.assertEqual(value["kind"], "Set")
        self.assertEqual(value["predicate"]["operator"], "|")
        self.assertEqual(value["predicate"]["left"]["left"]["binding"], value["binding"])
        self.assertEqual(value["predicate"]["right"]["left"]["binding"], value["binding"])

    def test_phase_failure_artifacts(self):
        cases = [("show 01.", {"diagnostics.json"}),
                 ("show 1", {"tokens.json", "diagnostics.json"}),
                 ("show missing.", {"tokens.json", "parser_out.json", "ast.json", "diagnostics.json"})]
        for text, expected in cases:
            result = compile_source(Source.from_text(text), table=TABLE, artifacts=True)
            self.assertEqual(set(result.artifacts), expected)

    def test_write_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / "tokens.json").mkdir()
            with self.assertRaises(Diagnostic) as caught:
                write_artifact(directory, "tokens.json", b"{}\n")
            self.assertEqual(caught.exception.status, 74)

    def test_cli_stdin(self):
        process = cli("-", data=b"show 3.\n")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(lf(process.stdout), (FIXTURES / "stdin.stdout").read_bytes())
        self.assertEqual(process.stderr, b"")

    def test_cli_raw_stdin_offsets(self):
        with tempfile.TemporaryDirectory() as directory:
            process = cli("--emit-dir", directory, "--phase", "lex", "-", data=b"show\r\n3.")
            self.assertEqual(process.returncode, 0, process.stderr)
            tokens = json.loads((Path(directory) / "tokens.json").read_bytes())["tokens"]
            self.assertEqual(tokens[1]["span"]["start"], {"line": 2, "column": 1, "offset": 6})

    def test_cli_usage_and_input_errors(self):
        for args, code in [((), 64), (("--unknown",), 64), (("--phase", "bad", "-"), 64),
                           (("--jobs", "0", "-"), 64), (("--jobs", "33", "-"), 64),
                           (("--jobs", "\uff14", "-"), 64), (("--emit-dir",), 64),
                           (("missing-source.set",), 66), (("--help",), 0), (("--version",), 0)]:
            with self.subTest(args=args):
                result = cli(*args)
                self.assertEqual(result.returncode, code, result.stderr)
                self.assertNotIn(b"Traceback", result.stderr)

    def test_cli_unavailable_output(self):
        with tempfile.TemporaryDirectory() as directory:
            process = cli("--emit-dir", str(Path(directory) / "missing"), "examples/arithmetic.set")
            self.assertEqual(process.returncode, 74)
            self.assertIn(b"I9074", process.stderr)

    def test_cli_output_directory_reuse(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "unrelated.txt").write_text("keep")
            result = cli("--emit-dir", directory, "tests/fixtures/compact.set")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(len(list(path.glob("*.json"))), 6)
            result = cli("--emit-dir", directory, "tests/fixtures/lex-error.set")
            self.assertEqual(result.returncode, 1)
            self.assertEqual([file.name for file in path.glob("*.json")], ["diagnostics.json"])
            self.assertEqual((path / "unrelated.txt").read_text(), "keep")

    def test_cli_parallel_dumps_and_trace(self):
        serial = cli("--dump-grammar", "--dump-slr", "--trace-parser", "examples/arithmetic.set")
        parallel = cli("--jobs", "4", "--dump-grammar", "--dump-slr", "--trace-parser", "examples/arithmetic.set")
        self.assertEqual(serial.returncode, 0, serial.stderr)
        self.assertEqual((serial.returncode, serial.stdout, serial.stderr),
                         (parallel.returncode, parallel.stdout, parallel.stderr))
        prefix = grammar_dump(TABLE.grammar, TABLE.analysis) + slr_dump(TABLE)
        self.assertTrue(lf(serial.stderr).startswith(prefix.encode()))

    def test_cli_file_bytes_and_phase_stopping(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "source.set"
            path.write_bytes(b"show\r\nmissing.")
            process = cli("--phase", "parse", str(path))
            self.assertEqual(process.returncode, 0)
            process = cli(str(path))
            self.assertEqual(process.returncode, 3)
            self.assertIn(b"2:1-2:8", process.stderr)

    def test_cli_rejects_gil_reenabled(self):
        import subprocess
        import sys
        from .common import ROOT
        process = subprocess.run([sys.executable, "-X", "gil=1", "-m", "uslc", "--version"],
                                 cwd=ROOT, capture_output=True, timeout=30)
        self.assertEqual(process.returncode, 64)
        self.assertIn(b"GIL disabled", process.stderr)
