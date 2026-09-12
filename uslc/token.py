"""Stable terminal IDs and source-slice tokens, matching the C implementation."""

from dataclasses import dataclass
from enum import IntEnum

from .diagnostic import Span
from .source import Source


class Kind(IntEnum):
    DOT = 0
    LET = 1
    ID = 2
    BE = 3
    INT = 4
    SET = 5
    UNION = 6
    PLUS = 7
    MINUS = 8
    INTERSECTION = 9
    STAR = 10
    NUM = 11
    LPAREN = 12
    RPAREN = 13
    LBRACE = 14
    RBRACE = 15
    COLON = 16
    OR = 17
    AND = 18
    NOT = 19
    LT = 20
    GT = 21
    EQ = 22
    MEMBER = 23
    SHOW = 24
    EOF = 25


SPELLINGS = (
    ".", "let", "id", "be", "int", "set", "U", "+", "-", "I", "*", "num",
    "(", ")", "{", "}", ":", "|", "&", "!", "<", ">", "=", "@", "show", "$",
)
INT64_MIN = -(1 << 63)
INT64_MAX = (1 << 63) - 1


@dataclass(frozen=True, slots=True)
class Token:
    kind: Kind
    span: Span
    number: int = 0

    def lexeme(self, source: Source) -> str:
        return source.data[self.span.start.offset:self.span.end.offset].decode("ascii")

    @property
    def spelling(self) -> str:
        return SPELLINGS[self.kind]
