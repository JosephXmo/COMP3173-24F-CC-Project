"""Command-line driver. No regular-expression based argument parser is used."""

from dataclasses import dataclass
from pathlib import Path
import sys

from .compiler import PHASES, compile_source
from .diagnostic import Diagnostic, Span, internal
from .grammar import analyze, core_grammar
from .json_writer import ARTIFACT_NAMES, diagnostics_json, encode_json
from .printer import diagnostic_report, grammar_dump, slr_dump
from .runtime import require_runtime
from .slr import build_slr
from .source import Source

HELP = """uslc 1.0.0 - UIC Set Language Compiler (Python)
usage: python -m uslc [options] <source-file|->
  --phase lex|parse|check|run  stop after a phase (default: run)
  --emit-dir <path>           write JSON to an existing directory
  --trace-parser              write shift/reduce trace to stderr
  --dump-grammar              write productions and FIRST/FOLLOW to stderr
  --dump-slr                  write LR(0) states and ACTION/GOTO to stderr
  --compact-json              omit JSON indentation
  --jobs <1..32>              parallel SLR workers (default: 1)
  --help                      show this help
  --version                   show version
  --                          end option parsing
Dump options may also be used without a source file.
"""


@dataclass(slots=True)
class Options:
    phase: str = "run"
    path: str | None = None
    emit_dir: str | None = None
    trace: bool = False
    grammar: bool = False
    slr: bool = False
    compact: bool = False
    workers: int = 1
    special: str | None = None


def parse_options(arguments: list[str]) -> Options:
    options = Options()
    positional = False
    iterator = iter(arguments)
    for argument in iterator:
        if not positional and argument == "--":
            positional = True
        elif not positional and argument in ("--help", "--version"):
            options.special = argument
            return options
        elif not positional and argument in ("--phase", "--emit-dir", "--jobs"):
            value = next(iterator, None)
            if value is None:
                raise ValueError("missing option value")
            if argument == "--phase":
                if value not in PHASES:
                    raise ValueError("invalid phase")
                options.phase = value
            elif argument == "--emit-dir":
                if not value:
                    raise ValueError("empty artifact directory")
                options.emit_dir = value
            else:
                if not value or len(value) > 2 or any(c < "0" or c > "9" for c in value):
                    raise ValueError("invalid worker count")
                options.workers = int(value)
                if not 1 <= options.workers <= 32:
                    raise ValueError("invalid worker count")
        elif not positional and argument == "--trace-parser":
            options.trace = True
        elif not positional and argument == "--dump-grammar":
            options.grammar = True
        elif not positional and argument == "--dump-slr":
            options.slr = True
        elif not positional and argument == "--compact-json":
            options.compact = True
        else:
            if (not positional and argument.startswith("-") and argument != "-") or options.path is not None:
                raise ValueError("invalid source or option")
            options.path = argument
    if options.path is None and not (options.grammar or options.slr):
        raise ValueError("missing source")
    return options


def output_error(path: str) -> Diagnostic:
    return Diagnostic(74, "I9074", "output", Span(),
                      f"cannot write output '{path}' (artifact directory must already exist)")


def prepare_artifacts(directory: str) -> None:
    for name in ARTIFACT_NAMES:
        path = Path(directory) / name
        try:
            path.unlink(missing_ok=True)
        except OSError as error:
            raise output_error(f"{directory}/{name}") from error


def write_artifact(directory: str, name: str, data: bytes) -> None:
    path = Path(directory) / name
    try:
        path.write_bytes(data)
    except OSError as error:
        try:
            path.unlink(missing_ok=True)
        except OSError:
            pass
        raise output_error(f"{directory}/{name}") from error


def main(arguments: list[str] | None = None) -> int:
    try:
        require_runtime()
    except RuntimeError as error:
        print(f"uslc: {error}", file=sys.stderr)
        return 64
    try:
        options = parse_options(sys.argv[1:] if arguments is None else arguments)
    except ValueError:
        print("uslc: invalid arguments; use --help", file=sys.stderr)
        return 64
    source = Source(options.path or "<none>", b"")
    artifact_ready = False
    try:
        if options.special:
            sys.stdout.write(HELP if options.special == "--help" else "uslc 1.0.0 (core language; Python)\n")
            sys.stdout.flush()
            return 0
        if options.emit_dir and options.path is not None:
            prepare_artifacts(options.emit_dir)
            artifact_ready = True
        if options.path is not None:
            try:
                source = Source("-", sys.stdin.buffer.read()) if options.path == "-" else Source.from_file(options.path)
            except (OSError, ValueError) as error:
                raise Diagnostic(66, "I9066", "input", Span(), f"cannot open source '{options.path}'") from error
        table = None
        if options.grammar or options.slr:
            grammar = core_grammar()
            analysis = analyze(grammar)
            if options.grammar:
                sys.stderr.write(grammar_dump(grammar, analysis))
            if options.slr:
                table = build_slr(grammar, options.workers)
                sys.stderr.write(slr_dump(table))
            sys.stderr.flush()
        if options.path is None:
            return 0
        result = compile_source(source, phase=options.phase, table=table, workers=options.workers,
                                artifacts=options.emit_dir is not None, compact=options.compact,
                                trace=options.trace)
        if result.trace:
            sys.stderr.write("".join(result.trace))
            sys.stderr.flush()
        if options.emit_dir:
            for name, data in result.artifacts.items():
                write_artifact(options.emit_dir, name, data)
        output = sys.stderr if result.status else sys.stdout
        output.write(result.report())
        output.flush()
        return result.status
    except Diagnostic as error:
        diagnostic = error
    except MemoryError:
        diagnostic = internal("I9001", "allocation or size limit exceeded")
    except RecursionError:
        diagnostic = internal("I9004", "tree nesting exceeds the implementation limit (256)")
    except (OSError, UnicodeError):
        return 74
    # Exception targets are cleared after an except clause; retain the diagnostic explicitly.
    return _report_failure(source, diagnostic, options, artifact_ready)


def _report_failure(source: Source, diagnostic: Diagnostic, options: Options, artifact_ready: bool) -> int:
    try:
        if artifact_ready and diagnostic.status != 74:
            try:
                data = encode_json(diagnostics_json(source, diagnostic), options.compact)
                write_artifact(options.emit_dir, "diagnostics.json", data)
            except Diagnostic as error:
                diagnostic = error
        sys.stderr.write(diagnostic_report(source, diagnostic))
        sys.stderr.flush()
    except (OSError, UnicodeError):
        return 74
    return diagnostic.status
