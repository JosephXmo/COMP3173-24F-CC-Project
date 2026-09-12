"""Pure structural lowering; only the later type checker mutates type and binding."""

from dataclasses import dataclass
from enum import StrEnum

from .diagnostic import Span, check_depth, internal
from .grammar import Nonterminal, core_grammar
from .parser import ParseNode
from .token import Token


class Type(StrEnum):
    VOID = "void"
    INTEGER = "integer"
    BOOLEAN = "boolean"
    SET = "set"
    ERROR = "error"


class NodeKind(StrEnum):
    PROGRAM = "Program"
    DECLARATION = "Declaration"
    SHOW = "ShowCommand"
    INTEGER = "IntegerLiteral"
    NAME = "Name"
    SET = "SetLiteral"
    ARITHMETIC = "BinaryArithmetic"
    SET_BINARY = "BinarySet"
    COMPARISON = "Comparison"
    MEMBERSHIP = "Membership"
    LOGICAL = "LogicalBinary"
    NOT = "LogicalNot"


@dataclass(slots=True)
class Ast:
    kind: NodeKind
    span: Span
    token: Token | None = None
    type: Type = Type.VOID
    declared_type: Type = Type.VOID
    binding: int | None = None
    left: Ast | None = None
    right: Ast | None = None
    declarations: tuple[Ast, ...] = ()


def lower_ast(root: ParseNode) -> Ast:
    grammar = core_grammar()

    def validate(node: ParseNode, depth: int) -> None:
        if not isinstance(node, ParseNode):
            raise internal("I9003", "missing parse tree child")
        check_depth(depth, node.span)
        if node.production is None:
            if (not node.children and node.token is not None and
                    0 <= node.symbol < grammar.terminals and node.symbol == node.token.kind):
                return
        elif 0 <= node.production < len(grammar.productions):
            rule = grammar.productions[node.production]
            if node.symbol == rule.lhs and len(node.children) == len(rule.rhs):
                for child, symbol in zip(node.children, rule.rhs):
                    validate(child, depth + 1)
                    if child.symbol != symbol:
                        raise internal("I9003", "parse tree child has wrong symbol", node.span)
                return
        raise internal("I9003", "parse tree does not match its production", node.span)

    def lower(node: ParseNode, depth: int) -> Ast:
        check_depth(depth, node.span)
        p, children = node.production, node.children
        if p in (8, 12, 21, 23, 24, 32, 33):
            return lower(children[0], depth + 1)
        if p in (17, 25):
            result = lower(children[1], depth + 1)
            result.span = node.span
            return result
        match p:
            case 1 | 2:
                declarations = []
                if p == 1:
                    current = children[0]
                    while True:
                        declarations.append(lower(current.children[0], depth + 1))
                        if current.production == 4:
                            break
                        current = current.children[1]
                return Ast(NodeKind.PROGRAM, node.span, declarations=tuple(declarations),
                           right=lower(children[1 if p == 1 else 0], depth + 1))
            case 5:
                return Ast(NodeKind.DECLARATION, node.span, children[2].token,
                           declared_type=Type.INTEGER if children[1].production == 6 else Type.SET,
                           left=lower(children[4], depth + 1))
            case 15 | 16:
                return Ast(NodeKind.INTEGER if p == 15 else NodeKind.NAME, node.span, children[0].token)
            case 18:
                return Ast(NodeKind.SET, node.span, children[1].children[0].token,
                           left=lower(children[2], depth + 1))
            case 26 | 31:
                return Ast(NodeKind.NOT if p == 26 else NodeKind.SHOW, node.span, children[0].token,
                           left=lower(children[1], depth + 1))
            case 9 | 13:
                kind = NodeKind.SET_BINARY
            case 10 | 11 | 14:
                kind = NodeKind.ARITHMETIC
            case 20 | 22:
                kind = NodeKind.LOGICAL
            case 27 | 28 | 29:
                kind = NodeKind.COMPARISON
            case 30:
                kind = NodeKind.MEMBERSHIP
            case _:
                raise internal("I9003", "unexpected production in AST lowering", node.span)
        return Ast(kind, node.span, children[1].token, left=lower(children[0], depth + 1),
                   right=lower(children[2], depth + 1))

    validate(root, 0)
    if root.symbol != Nonterminal.S:
        raise internal("I9003", "expected program root", root.span)
    return lower(root, 0)
