"""Per-compilation lexical scopes. No process-global mutable symbol state."""

from dataclasses import dataclass

from .ast import Type
from .token import Token


@dataclass(frozen=True, slots=True)
class Symbol:
    name: str
    token: Token
    type: Type
    binding: int


class SymbolTable:
    def __init__(self):
        self.scopes: list[dict[str, Symbol]] = [{}]

    def find(self, name: str) -> Symbol | None:
        for scope in reversed(self.scopes):
            if name in scope:
                return scope[name]
        return None

    def insert(self, symbol: Symbol) -> None:
        self.scopes[-1][symbol.name] = symbol

    @property
    def globals(self) -> tuple[Symbol, ...]:
        return tuple(self.scopes[0].values())
