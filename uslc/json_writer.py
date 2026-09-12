"""C-compatible ordered artifact schemas and UTF-8 JSON encoding."""

import json

from .ast import Ast, NodeKind
from .diagnostic import Diagnostic, Position, Span
from .evaluator import Value
from .grammar import Grammar
from .parser import ParseNode
from .source import Source
from .symbol_table import Symbol
from .token import SPELLINGS, Kind, Token

ARTIFACT_NAMES = ("tokens.json", "parser_out.json", "ast.json", "typing_out.json",
                  "evaluation_out.json", "diagnostics.json")


def encode_json(document: object, compact: bool = False) -> bytes:
    options = {"ensure_ascii": False, "allow_nan": False}
    if compact:
        options["separators"] = (",", ":")
    else:
        options["indent"] = 2
    return (json.dumps(document, **options) + "\n").encode("utf-8")


def position_json(position: Position) -> dict:
    return {"line": position.line, "column": position.column, "offset": position.offset}


def span_json(span: Span) -> dict:
    return {"start": position_json(span.start), "end": position_json(span.end)}


def token_json(source: Source, token: Token) -> dict:
    result = {"kind": token.kind.name, "lexeme": token.lexeme(source), "span": span_json(token.span)}
    if token.kind == Kind.NUM:
        result["value"] = token.number
    return result


def tokens_json(source: Source, tokens: tuple[Token, ...]) -> dict:
    return {"schema": "uslc.tokens.v1", "source": source.name,
            "tokens": [token_json(source, token) for token in tokens]}


def parse_tree_json(source: Source, grammar: Grammar, root: ParseNode) -> dict:
    def node_json(node: ParseNode) -> dict:
        result = {"symbol": grammar.names[node.symbol], "production_id": node.production,
                  "span": span_json(node.span)}
        if node.token is not None:
            result["token"] = token_json(source, node.token)
        result["children"] = [node_json(child) for child in node.children]
        return result
    return {"schema": "uslc.parser.v1", "source": source.name, "root": node_json(root)}


def ast_json(source: Source, root: Ast, symbols: tuple[Symbol, ...] | None = None) -> dict:
    def node_json(node: Ast) -> dict:
        result = {"kind": node.kind.value, "span": span_json(node.span), "type": node.type.value}
        if node.binding is not None:
            result["binding"] = node.binding
        if node.kind in (NodeKind.DECLARATION, NodeKind.NAME, NodeKind.SET):
            result["name"] = node.token.lexeme(source)
        if node.kind == NodeKind.DECLARATION:
            result["declared_type"] = node.declared_type.value
        if node.kind == NodeKind.INTEGER:
            result["value"] = node.token.number
        if node.kind in (NodeKind.ARITHMETIC, NodeKind.SET_BINARY, NodeKind.COMPARISON,
                         NodeKind.MEMBERSHIP, NodeKind.LOGICAL, NodeKind.NOT):
            result["operator"] = node.token.spelling
        children = [node_json(child) for child in node.declarations]
        if node.left:
            children.append(node_json(node.left))
        if node.right:
            children.append(node_json(node.right))
        result["children"] = children
        return result
    result = {"schema": "uslc.ast.v1" if symbols is None else "uslc.typing.v1", "source": source.name}
    if symbols is not None:
        result["status"] = "success"
        result["symbols"] = [{"name": symbol.name, "type": symbol.type.value, "binding": symbol.binding}
                             for symbol in symbols]
    result["root"] = node_json(root)
    return result


def symbolic_json(value: Value) -> dict:
    result = {"kind": value.kind, "type": value.type.value}
    match value.kind:
        case "Integer":
            result["value"] = value.integer
        case "Boolean":
            result["value"] = value.boolean
        case "Variable":
            result["binding"] = value.binding
        case "Set":
            result["binding"] = value.binding
            result["predicate"] = symbolic_json(value.left)
        case _:
            result["operator"] = SPELLINGS[value.op]
            result["left"] = symbolic_json(value.left)
            if value.right:
                result["right"] = symbolic_json(value.right)
    return result


def evaluation_json(value: Value, display: str) -> dict:
    result = {"type": value.type.value}
    if value.kind == "Integer":
        result["value"] = value.integer
    elif value.kind == "Boolean":
        result["value"] = value.boolean
    else:
        result["value"] = symbolic_json(value)
        result["display"] = display
    return {"schema": "uslc.evaluation.v1", "status": "success", "result": result}


def diagnostics_json(source: Source, diagnostic: Diagnostic | None) -> dict:
    entries = [] if diagnostic is None else [{"code": diagnostic.code, "phase": diagnostic.phase,
                                            "message": diagnostic.message, "span": span_json(diagnostic.span)}]
    return {"schema": "uslc.diagnostics.v1", "source": source.name, "diagnostics": entries}
