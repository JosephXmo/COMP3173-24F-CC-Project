"""Stable human-readable reports, symbolic display, and grammar/parser dumps."""

from .diagnostic import Diagnostic, check_depth, internal
from .evaluator import Value
from .grammar import Analysis, Grammar
from .slr import SlrTable
from .source import Source
from .token import SPELLINGS


def representative(depth: int) -> str:
    name = "x"
    while depth:
        name += chr(97 + (depth - 1) % 26)
        depth = (depth - 1) // 26
    return name


def value_display(value: Value) -> str:
    def display(node: Value, scope: tuple[int, ...], depth: int) -> str:
        check_depth(depth, node.span)
        match node.kind:
            case "Integer":
                return str(node.integer)
            case "Boolean":
                return "true" if node.boolean else "false"
            case "Variable":
                for index in range(len(scope) - 1, -1, -1):
                    if scope[index] == node.binding:
                        return representative(index)
                raise internal("E4002", "unbound representative in display", node.span)
            case "Set":
                return ("{ " + representative(len(scope)) + " : "
                        + display(node.left, scope + (node.binding,), depth + 1) + " }")
            case "Not":
                return "!(" + display(node.left, scope, depth + 1) + ")"
            case _:
                return ("(" + display(node.left, scope, depth + 1) + " " + SPELLINGS[node.op]
                        + " " + display(node.right, scope, depth + 1) + ")")
    return display(value, (), 0)


def production_text(grammar: Grammar, index: int, dot: int | None = None) -> str:
    production = grammar.productions[index]
    result = f"{index}. {grammar.names[production.lhs]} ->"
    for i in range(len(production.rhs) + 1):
        if i == dot:
            result += " ."
        if i < len(production.rhs):
            result += " " + grammar.names[production.rhs[i]]
    if not production.rhs and dot is None:
        result += " epsilon"
    return result + "\n"


def grammar_dump(grammar: Grammar, analysis: Analysis) -> str:
    result = "".join(production_text(grammar, i) for i in range(len(grammar.productions)))
    for symbol in range(grammar.terminals, len(grammar.names)):
        first = ", ".join(grammar.names[i] for i in sorted(analysis.first[symbol]))
        follow = ", ".join(grammar.names[i] for i in sorted(analysis.follow[symbol]))
        nullable = "true" if analysis.nullable[symbol] else "false"
        result += f"{grammar.names[symbol]}: nullable={nullable} FIRST={{{first}}} FOLLOW={{{follow}}}\n"
    return result


def slr_dump(table: SlrTable) -> str:
    lines = []
    for index, state in enumerate(table.states):
        lines.append(f"state {index}:\n")
        for production, dot in sorted(state.items):
            lines.append("  " + production_text(table.grammar, production, dot))
        for terminal, action in enumerate(state.actions):
            if action.kind != "error":
                lines.append(f"  ACTION[{table.grammar.names[terminal]}] = {action.kind} {action.target}\n")
        for symbol in range(table.grammar.terminals, len(table.grammar.names)):
            target = state.transitions[symbol]
            if target is not None:
                lines.append(f"  GOTO[{table.grammar.names[symbol]}] = {target}\n")
    return "".join(lines)


def diagnostic_report(source: Source, diagnostic: Diagnostic) -> str:
    start, end = diagnostic.span.start, diagnostic.span.end
    result = (f"UIC Set Language Compiler\nsource: {source.name}\nstatus: error\n\ndiagnostic:\n"
              f"  phase:    {diagnostic.phase}\n  code:     {diagnostic.code}\n"
              f"  location: {start.line}:{start.column}-{end.line}:{end.column}\n"
              f"  message:  {diagnostic.message}\n")
    if start.line and start.offset <= len(source.data):
        begin = finish = start.offset
        while begin and source.data[begin - 1] not in (10, 13):
            begin -= 1
        while finish < len(source.data) and source.data[finish] not in (10, 13):
            finish += 1
        excerpt = "".join(chr(c) if 32 <= c < 127 else " " for c in source.data[begin:finish])
        result += (f"\n{start.line} | {excerpt}\n" + " " * len(str(start.line)) + " | "
                   + " " * (start.offset - begin) + "^\n")
    return result


def success_report(source: Source, completed: int, token_count: int, node_count: int,
                   value: Value | None, display: str | None) -> str:
    result = f"UIC Set Language Compiler\nsource: {source.name}\nstatus: success\n\nphases:\n"
    if completed >= 1:
        result += f"  lexical:  ok ({token_count} tokens)\n"
    if completed >= 2:
        result += f"  syntax:   ok ({node_count} parse nodes)\n"
    if completed >= 3:
        result += "  typing:   ok\n"
    if completed >= 4:
        result += "  evaluate: ok\n"
    if value is not None:
        result += f"\nresult:\n  type:  {value.type}\n  value: {display}\n"
    return result
