"""Byte-by-byte maximal-munch scanner; no regular expressions or Unicode classes."""

from .diagnostic import Diagnostic, Position, Span
from .source import Source
from .token import INT64_MAX, SPELLINGS, Kind, Token


class Lexer:
    def __init__(self, source: Source):
        self.source = source
        self.offset = 0
        self.line = 1
        self.column = 1
        self.previous_cr = False

    @property
    def position(self) -> Position:
        return Position(self.line, self.column, self.offset)

    def advance(self) -> None:
        byte = self.source.data[self.offset]
        self.offset += 1
        if byte == 13:
            self.line += 1
            self.column = 1
        elif byte == 10:
            if not self.previous_cr:
                self.line += 1
            self.column = 1
        else:
            self.column += 1
        self.previous_cr = byte == 13

    def next_token(self) -> Token:
        data = self.source.data
        while self.offset < len(data) and data[self.offset] in b" \t\r\n\f\v":
            self.advance()
        start = self.position
        if self.offset == len(data):
            return Token(Kind.EOF, Span(start, start))
        byte = data[self.offset]
        number = 0
        error = None
        if 97 <= byte <= 122:
            while self.offset < len(data) and 97 <= data[self.offset] <= 122:
                self.advance()
            word = data[start.offset:self.offset]
            kind = Kind.ID
            for keyword in (Kind.LET, Kind.BE, Kind.INT, Kind.SET, Kind.SHOW):
                if word == SPELLINGS[keyword].encode("ascii"):
                    kind = keyword
                    break
        elif 48 <= byte <= 57:
            overflow = False
            while self.offset < len(data) and 48 <= data[self.offset] <= 57:
                digit = data[self.offset] - 48
                overflow = overflow or number > (INT64_MAX - digit) // 10
                if not overflow:
                    number = number * 10 + digit
                self.advance()
            kind = Kind.NUM
            if byte == 48 and self.offset - start.offset > 1:
                error = ("L1002", "leading zero in integer literal")
            elif overflow:
                error = ("L1003", "integer literal exceeds int64_t range")
        else:
            self.advance()
            kind = None
            for candidate in Kind:
                word = SPELLINGS[candidate]
                if candidate != Kind.EOF and len(word) == 1 and ord(word) == byte:
                    kind = candidate
                    break
            if kind is None:
                error = ("L1001", "invalid character")
        span = Span(start, self.position)
        if error:
            raise Diagnostic(1, error[0], "lexical", span, error[1])
        return Token(kind, span, number)


def lex(source: Source) -> tuple[Token, ...]:
    lexer = Lexer(source)
    tokens = []
    while True:
        token = lexer.next_token()
        tokens.append(token)
        if token.kind == Kind.EOF:
            return tuple(tokens)
