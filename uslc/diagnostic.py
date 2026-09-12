"""Source positions and fail-fast structured diagnostics."""

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class Position:
    line: int = 0
    column: int = 0
    offset: int = 0


@dataclass(frozen=True, slots=True)
class Span:
    start: Position = Position()
    end: Position = Position()


class Diagnostic(Exception):
    def __init__(self, status: int, code: str, phase: str, span: Span, message: str):
        super().__init__(message)
        self.status = status
        self.code = code
        self.phase = phase
        self.span = span
        self.message = message


def internal(code: str, message: str, span: Span = Span()) -> Diagnostic:
    return Diagnostic(70, code, "internal", span, message)


def check_depth(depth: int, span: Span) -> None:
    if depth >= 256:
        raise internal("I9004", "tree nesting exceeds the implementation limit (256)", span)
