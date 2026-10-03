"""Scoped, fail-fast static semantics. Initializers precede declaration visibility."""

from .ast import Ast, NodeKind, Type
from .diagnostic import Diagnostic, check_depth, internal
from .source import Source
from .symbol_table import Symbol, SymbolTable


def type_check(program: Ast, source: Source) -> tuple[tuple[Symbol, ...], int]:
    table = SymbolTable()
    next_binding = 0

    def bind(node: Ast) -> Symbol:
        nonlocal next_binding
        node.binding = next_binding
        next_binding += 1
        return Symbol(node.token.lexeme(source), node.token,
                      Type.INTEGER if node.kind == NodeKind.SET else node.declared_type, node.binding)

    def check(node: Ast, depth: int = 0) -> None:
        check_depth(depth, node.span)
        if node.kind == NodeKind.INTEGER:
            node.type = Type.INTEGER
            return
        if node.kind == NodeKind.NAME:
            symbol = table.find(node.token.lexeme(source))
            if symbol is None:
                raise Diagnostic(3, "T3001", "typing", node.token.span, "undefined identifier")
            node.type, node.binding = symbol.type, symbol.binding
            return
        if node.kind == NodeKind.SET:
            table.scopes.append({})
            try:
                table.insert(bind(node))
                check(node.left, depth + 1)
            finally:
                table.scopes.pop()
            if node.left.type != Type.BOOLEAN:
                raise Diagnostic(3, "T3003", "typing", node.span, "set predicate must be boolean")
            node.type = Type.SET
            return
        check(node.left, depth + 1)
        if node.right is not None:
            check(node.right, depth + 1)
        left = right = result = Type.INTEGER
        match node.kind:
            case NodeKind.ARITHMETIC:
                pass
            case NodeKind.SET_BINARY:
                left = right = result = Type.SET
            case NodeKind.COMPARISON:
                result = Type.BOOLEAN
            case NodeKind.MEMBERSHIP:
                right, result = Type.SET, Type.BOOLEAN
            case NodeKind.LOGICAL | NodeKind.NOT:
                left = right = result = Type.BOOLEAN
            case _:
                raise internal("I9003", "invalid expression node in checker", node.span)
        if node.left.type != left or (node.right is not None and node.right.type != right):
            node.type = Type.ERROR
            expected = f"{left} and {right}" if node.right else str(left)
            found = f"{node.left.type} and {node.right.type}" if node.right else str(node.left.type)
            raise Diagnostic(3, "T3003", "typing", node.token.span,
                             f"operator '{node.token.spelling}' expects {expected}; found {found}")
        node.type = result

    for declaration in program.declarations:
        previous = table.find(declaration.token.lexeme(source))
        if previous:
            pos = previous.token.span.start
            raise Diagnostic(3, "T3002", "typing", declaration.token.span,
                             f"duplicate declaration (previous declaration at {pos.line}:{pos.column})")
        check(declaration.left)
        if declaration.left.type != declaration.declared_type:
            raise Diagnostic(3, "T3004", "typing", declaration.left.span,
                             f"initializer has type {declaration.left.type}; "
                             f"declaration requires {declaration.declared_type}")
        table.insert(bind(declaration))
    check(program.right.left)
    return table.globals, next_binding
