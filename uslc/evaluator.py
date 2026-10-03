"""Immutable symbolic values, checked int64 arithmetic, and lazy set predicates."""

from dataclasses import dataclass, replace

from .ast import Ast, NodeKind, Type
from .diagnostic import Diagnostic, Span, check_depth, internal
from .token import INT64_MAX, INT64_MIN, SPELLINGS, Kind


@dataclass(frozen=True, slots=True, eq=False)
class Value:
    kind: str
    type: Type
    span: Span = Span()
    op: Kind | None = None
    integer: int = 0
    boolean: bool = False
    binding: int | None = None
    left: Value | None = None
    right: Value | None = None


def checked_arithmetic(op: Kind, left: int, right: int, span: Span = Span()) -> int:
    if not (INT64_MIN <= left <= INT64_MAX and INT64_MIN <= right <= INT64_MAX):
        raise internal("E4090", "integer operand is outside int64_t range", span)
    match op:
        case Kind.PLUS:
            result = left + right
        case Kind.MINUS:
            result = left - right
        case Kind.STAR:
            result = left * right
        case _:
            raise internal("E4090", "invalid arithmetic operator", span)
    if not INT64_MIN <= result <= INT64_MAX:
        raise Diagnostic(4, "E4001", "evaluate", span,
                         f"integer overflow in operator '{SPELLINGS[op]}'")
    return result


def substitute(value: Value, binding: int, replacement: Value, depth: int = 0) -> Value:
    check_depth(depth, value.span)
    if value.kind == "Variable" and value.binding == binding:
        return replacement
    left = substitute(value.left, binding, replacement, depth + 1) if value.left else None
    right = substitute(value.right, binding, replacement, depth + 1) if value.right else None
    if left is value.left and right is value.right:
        return value
    return replace(value, left=left, right=right)


class Evaluator:
    def __init__(self, binding_count: int):
        self.environment: dict[int, Value] = {}
        self.binding_count = binding_count
        self.next_binding = binding_count

    def close(self, node: Ast, depth: int = 0) -> Value:
        if node is None or node.type in (Type.VOID, Type.ERROR):
            raise internal("E4090", "expected a type-checked expression", node.span if node else Span())
        check_depth(depth, node.span)
        if node.kind == NodeKind.NAME:
            if node.binding not in self.environment:
                raise internal("E4090", "unbound identifier after type checking", node.span)
            return self.environment[node.binding]
        if node.kind == NodeKind.INTEGER:
            return Value("Integer", node.type, node.token.span, integer=node.token.number)
        if node.kind == NodeKind.SET:
            if node.binding is None or not 0 <= node.binding < self.binding_count:
                raise internal("E4090", "invalid representative binding", node.span)
            saved = self.environment.get(node.binding)
            self.environment[node.binding] = Value("Variable", Type.INTEGER, node.token.span,
                                                   binding=node.binding)
            try:
                predicate = self.close(node.left, depth + 1)
            finally:
                if saved is None:
                    del self.environment[node.binding]
                else:
                    self.environment[node.binding] = saved
            return Value("Set", Type.SET, node.span, binding=node.binding, left=predicate)
        if node.kind not in (NodeKind.ARITHMETIC, NodeKind.SET_BINARY, NodeKind.COMPARISON,
                             NodeKind.MEMBERSHIP, NodeKind.LOGICAL, NodeKind.NOT):
            raise internal("E4090", "expected a type-checked expression", node.span)
        # Capturing a predicate never evaluates its arithmetic or its conditional branches.
        left = self.close(node.left, depth + 1)
        right = self.close(node.right, depth + 1) if node.right else None
        return Value("Not" if node.kind == NodeKind.NOT else "Binary", node.type, node.token.span,
                     node.token.kind, left=left, right=right)

    def reduce(self, value: Value, depth: int = 0) -> Value:
        check_depth(depth, value.span)
        if value.kind in ("Integer", "Boolean", "Set"):
            return value
        if value.kind == "Variable":
            raise internal("E4002", "unbound set representative", value.span)
        if value.left is None or (value.kind == "Binary" and value.right is None):
            raise internal("E4090", "missing evaluator operand", value.span)
        left = self.reduce(value.left, depth + 1)
        expected_left = expected_right = Type.INTEGER
        if value.op in (Kind.AND, Kind.OR, Kind.NOT):
            expected_left = expected_right = Type.BOOLEAN
        elif value.op in (Kind.UNION, Kind.INTERSECTION):
            expected_left = expected_right = Type.SET
        elif value.op == Kind.MEMBER:
            expected_right = Type.SET
        if left.type != expected_left:
            raise internal("E4090", "invalid left operand after type checking", value.span)
        if (value.op == Kind.AND and not left.boolean) or (value.op == Kind.OR and left.boolean):
            return left
        right = self.reduce(value.right, depth + 1) if value.right else None
        if right is not None and right.type != expected_right:
            raise internal("E4090", "invalid right operand after type checking", value.span)
        if value.op == Kind.MEMBER:
            predicate = substitute(right.left, right.binding, left, depth + 1)
            return self.reduce(predicate, depth + 1)
        if value.op in (Kind.UNION, Kind.INTERSECTION):
            variable = Value("Variable", Type.INTEGER, value.span, binding=self.next_binding)
            self.next_binding += 1
            predicate = Value("Binary", Type.BOOLEAN, value.span,
                              Kind.OR if value.op == Kind.UNION else Kind.AND,
                              left=substitute(left.left, left.binding, variable, depth + 1),
                              right=substitute(right.left, right.binding, variable, depth + 1))
            return Value("Set", Type.SET, value.span, binding=variable.binding, left=predicate)
        if value.op in (Kind.PLUS, Kind.MINUS, Kind.STAR):
            result = checked_arithmetic(value.op, left.integer, right.integer, value.span)
            return Value("Integer", Type.INTEGER, value.span, integer=result)
        match value.op:
            case Kind.LT:
                result = left.integer < right.integer
            case Kind.GT:
                result = left.integer > right.integer
            case Kind.EQ:
                result = left.integer == right.integer
            case Kind.AND:
                result = left.boolean and right.boolean
            case Kind.OR:
                result = left.boolean or right.boolean
            case Kind.NOT:
                result = not left.boolean
            case _:
                raise internal("E4090", "invalid evaluator operator", value.span)
        return Value("Boolean", Type.BOOLEAN, value.span, boolean=result)


def evaluate(program: Ast, binding_count: int) -> Value:
    if (program.kind != NodeKind.PROGRAM or program.right is None
            or program.right.kind != NodeKind.SHOW):
        raise internal("E4090", "expected a program with one show command")
    evaluator = Evaluator(binding_count)
    for declaration in program.declarations:
        if declaration.binding is None or not 0 <= declaration.binding < binding_count:
            raise internal("E4090", "invalid declaration binding", declaration.span)
        value = evaluator.reduce(evaluator.close(declaration.left))
        evaluator.environment[declaration.binding] = value
    return evaluator.reduce(evaluator.close(program.right.left))
