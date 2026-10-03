"""Independent compilation sessions and ordered, free-threaded batch compilation."""

from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field

from .ast import Ast, lower_ast
from .diagnostic import Diagnostic, internal
from .evaluator import Value, evaluate
from .grammar import core_grammar
from .json_writer import (ast_json, diagnostics_json, encode_json, evaluation_json,
                          parse_tree_json, tokens_json)
from .lexer import lex
from .parser import ParseResult, parse
from .printer import diagnostic_report, success_report, value_display
from .runtime import default_workers
from .slr import SlrTable, build_slr
from .source import Source
from .symbol_table import Symbol
from .token import Token
from .type_checker import type_check

PHASES = ("lex", "parse", "check", "run")


@dataclass(slots=True)
class Compilation:
    source: Source
    completed: int = 0
    tokens: tuple[Token, ...] = ()
    parsed: ParseResult | None = None
    ast: Ast | None = None
    symbols: tuple[Symbol, ...] = ()
    value: Value | None = None
    display: str | None = None
    diagnostic: Diagnostic | None = None
    artifacts: dict[str, bytes] = field(default_factory=dict)
    trace: list[str] = field(default_factory=list)

    @property
    def status(self) -> int:
        return self.diagnostic.status if self.diagnostic else 0

    def report(self) -> str:
        if self.diagnostic:
            return diagnostic_report(self.source, self.diagnostic)
        return success_report(self.source, self.completed, len(self.tokens),
                              self.parsed.node_count if self.parsed else 0, self.value, self.display)


def compile_source(source: Source, *, phase: str = "run", table: SlrTable | None = None,
                   workers: int = 1, artifacts: bool = False, compact: bool = False,
                   trace: bool = False) -> Compilation:
    if phase not in PHASES:
        raise ValueError(f"unknown compilation phase: {phase}")
    result = Compilation(source)

    def save(name: str, factory, *args) -> None:
        if artifacts:
            result.artifacts[name] = encode_json(factory(*args), compact)

    try:
        result.tokens = lex(source)
        result.completed = 1
        save("tokens.json", tokens_json, source, result.tokens)
        if phase != "lex":
            table = table or build_slr(core_grammar(), workers)
            result.parsed = parse(table, result.tokens, result.trace if trace else None)
            result.completed = 2
            save("parser_out.json", parse_tree_json, source, table.grammar, result.parsed.root)
            result.ast = lower_ast(result.parsed.root)
            save("ast.json", ast_json, source, result.ast)
            if phase != "parse":
                result.symbols, binding_count = type_check(result.ast, source)
                result.completed = 3
                save("typing_out.json", ast_json, source, result.ast, result.symbols)
                if phase == "run":
                    result.value = evaluate(result.ast, binding_count)
                    result.display = value_display(result.value)
                    result.completed = 4
                    save("evaluation_out.json", evaluation_json, result.value, result.display)
    except Diagnostic as diagnostic:
        result.diagnostic = diagnostic
    except MemoryError:
        result.diagnostic = internal("I9001", "allocation or size limit exceeded")
    except RecursionError:
        result.diagnostic = internal("I9004", "tree nesting exceeds the implementation limit (256)")
    if artifacts:
        try:
            save("diagnostics.json", diagnostics_json, source, result.diagnostic)
        except (MemoryError, RecursionError):
            result.diagnostic = internal("I9001", "cannot allocate diagnostic artifact")
    return result


def compile_many(sources: tuple[Source, ...] | list[Source], *, workers: int | None = None,
                 phase: str = "run", artifacts: bool = False, compact: bool = False) -> list[Compilation]:
    """Compile independently; return input order, with no worker writing files or stdout."""
    workers = default_workers() if workers is None else workers
    if workers < 1:
        raise ValueError("workers must be positive")
    if phase not in PHASES:
        raise ValueError(f"unknown compilation phase: {phase}")
    if not sources:
        return []
    table = build_slr(core_grammar()) if phase != "lex" else None

    def run(source: Source) -> Compilation:
        return compile_source(source, table=table, phase=phase, artifacts=artifacts, compact=compact)

    if workers == 1 or len(sources) == 1:
        return [run(source) for source in sources]
    with ThreadPoolExecutor(max_workers=min(workers, len(sources))) as pool:
        return list(pool.map(run, sources))
