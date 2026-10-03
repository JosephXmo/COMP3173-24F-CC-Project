"""Table-driven shift/reduce parser, independent of semantics and evaluation."""

from dataclasses import dataclass

from .diagnostic import Diagnostic, Span, check_depth, internal
from .slr import SlrTable
from .token import Token


@dataclass(frozen=True, slots=True)
class ParseNode:
    symbol: int
    production: int | None
    span: Span
    children: tuple[ParseNode, ...] = ()
    token: Token | None = None
    height: int = 1


@dataclass(frozen=True, slots=True)
class ParseResult:
    root: ParseNode
    node_count: int
    trace: tuple[str, ...]


def parse(table: SlrTable, tokens: tuple[Token, ...], trace: list[str] | None = None) -> ParseResult:
    grammar = table.grammar
    if not tokens or tokens[-1].kind != grammar.eof:
        raise internal("I9003", "invalid parser state or token stream")
    states = [0]
    nodes = []
    cursor = 0
    node_count = 0
    while cursor < len(tokens):
        token = tokens[cursor]
        state = table.states[states[-1]]
        if not 0 <= token.kind < grammar.terminals:
            break
        action = state.actions[token.kind]
        if trace is not None:
            start = token.span.start
            trace.append(f"state {states[-1]}, {token.kind.name} at {start.line}:{start.column}: "
                         f"{action.kind} {action.target}\n")
        if action.kind == "accept":
            if len(nodes) != 1 or cursor != len(tokens) - 1:
                break
            return ParseResult(nodes[0], node_count, tuple(trace or ()))
        if action.kind == "error":
            expected = ", ".join(grammar.names[i] for i, cell in enumerate(state.actions)
                                 if cell.kind != "error")
            code = "P2002" if token.kind == grammar.eof else "P2001"
            raise Diagnostic(2, code, "syntax", token.span,
                             f"unexpected {token.kind.name}; expected: {expected}")
        node_count += 1
        if action.kind == "shift":
            node = ParseNode(int(token.kind), None, token.span, token=token)
            target = action.target
            cursor += 1
        else:
            production = grammar.productions[action.target]
            count = len(production.rhs)
            if count >= len(states):
                break
            children = tuple(nodes[-count:]) if count else ()
            if count:
                del nodes[-count:]
                del states[-count:]
            span = Span(children[0].span.start, children[-1].span.end) if children else Span(token.span.start, token.span.start)
            height = 1 + max((child.height for child in children), default=0)
            check_depth(height, span)
            node = ParseNode(production.lhs, action.target, span, children, height=height)
            target = table.states[states[-1]].transitions[production.lhs]
            if target is None:
                break
        nodes.append(node)
        states.append(target)
    raise internal("I9003", "invalid parser state or token stream")
