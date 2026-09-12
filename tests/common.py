from pathlib import Path
import subprocess
import sys

from uslc.compiler import compile_source
from uslc.grammar import core_grammar
from uslc.slr import build_slr
from uslc.source import Source

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures"
TABLE = build_slr(core_grammar())


def compile_text(text: str, **options):
    return compile_source(Source.from_text(text, "test.set"), table=TABLE, **options)


def cli(*arguments: str, data: bytes | None = None, cwd: Path = ROOT) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, "-m", "uslc", *map(str, arguments)],
                          cwd=cwd, input=data, capture_output=True, timeout=30)


def lf(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n")
