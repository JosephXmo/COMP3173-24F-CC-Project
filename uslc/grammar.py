"""Grammar validation and textbook nullable/FIRST/FOLLOW fixed points."""

from dataclasses import dataclass
from enum import IntEnum

from .diagnostic import internal
from .token import SPELLINGS, Kind


class Nonterminal(IntEnum):
    AUGMENTED = 26
    S = 27
    DECLS = 28
    D = 29
    T = 30
    E = 31
    TERM = 32
    FACTOR = 33
    Z = 34
    P = 35
    CONJ = 36
    PRED = 37
    R = 38
    C = 39
    A = 40


@dataclass(frozen=True, slots=True)
class Production:
    lhs: int
    rhs: tuple[int, ...]


@dataclass(frozen=True, slots=True)
class Grammar:
    names: tuple[str, ...]
    terminals: int
    eof: int
    productions: tuple[Production, ...]

    def validate(self) -> None:
        if not (0 < self.terminals <= 32 and self.terminals < len(self.names) <= 64
                and 0 <= self.eof < self.terminals and 0 < len(self.productions) <= 64):
            raise internal("I9002", "invalid grammar dimensions")
        if any(not name for name in self.names):
            raise internal("I9002", "missing grammar symbol name")
        augmented = self.productions[0]
        for index, production in enumerate(self.productions):
            if not self.terminals <= production.lhs < len(self.names) or len(production.rhs) > 8:
                raise internal("I9002", f"invalid production {index}")
            if any(not 0 <= symbol < len(self.names) or symbol == augmented.lhs
                   for symbol in production.rhs):
                raise internal("I9002", f"invalid RHS in production {index}")
            if index and production.lhs == augmented.lhs:
                raise internal("I9002", "augmented start must have one production")
        if len(augmented.rhs) != 1 or augmented.rhs[0] < self.terminals:
            raise internal("I9002", "invalid augmented start production")


@dataclass(frozen=True, slots=True)
class Analysis:
    nullable: tuple[bool, ...]
    first: tuple[frozenset[int], ...]
    follow: tuple[frozenset[int], ...]


def analyze(grammar: Grammar) -> Analysis:
    grammar.validate()
    nullable = [False] * len(grammar.names)
    first = [{i} if i < grammar.terminals else set() for i in range(len(grammar.names))]
    follow = [set() for _ in grammar.names]
    changed = True
    while changed:
        changed = False
        for production in grammar.productions:
            before = len(first[production.lhs])
            all_nullable = True
            for symbol in production.rhs:
                first[production.lhs].update(first[symbol])
                if not nullable[symbol]:
                    all_nullable = False
                    break
            if all_nullable and not nullable[production.lhs]:
                nullable[production.lhs] = True
                changed = True
            changed |= before != len(first[production.lhs])
    follow[grammar.productions[0].lhs].add(grammar.eof)
    changed = True
    while changed:
        changed = False
        for production in grammar.productions:
            trailer = follow[production.lhs].copy()
            for symbol in reversed(production.rhs):
                if symbol >= grammar.terminals:
                    before = len(follow[symbol])
                    follow[symbol].update(trailer)
                    changed |= before != len(follow[symbol])
                trailer = trailer | first[symbol] if nullable[symbol] else first[symbol].copy()
    return Analysis(tuple(nullable), tuple(map(frozenset, first)), tuple(map(frozenset, follow)))


def core_grammar() -> Grammar:
    n, k = Nonterminal, Kind
    definitions = (
        (n.AUGMENTED, (n.S,)), (n.S, (n.DECLS, n.C, k.DOT)), (n.S, (n.C, k.DOT)),
        (n.DECLS, (n.D, n.DECLS)), (n.DECLS, (n.D,)),
        (n.D, (k.LET, n.T, k.ID, k.BE, n.E, k.DOT)), (n.T, (k.INT,)), (n.T, (k.SET,)),
        (n.E, (n.TERM,)), (n.E, (n.E, k.UNION, n.TERM)),
        (n.E, (n.E, k.PLUS, n.TERM)), (n.E, (n.E, k.MINUS, n.TERM)),
        (n.TERM, (n.FACTOR,)), (n.TERM, (n.TERM, k.INTERSECTION, n.FACTOR)),
        (n.TERM, (n.TERM, k.STAR, n.FACTOR)), (n.FACTOR, (k.NUM,)), (n.FACTOR, (k.ID,)),
        (n.FACTOR, (k.LPAREN, n.E, k.RPAREN)), (n.FACTOR, (k.LBRACE, n.Z, n.P, k.RBRACE)),
        (n.Z, (k.ID, k.COLON)), (n.P, (n.P, k.OR, n.CONJ)), (n.P, (n.CONJ,)),
        (n.CONJ, (n.CONJ, k.AND, n.PRED)), (n.CONJ, (n.PRED,)), (n.PRED, (n.R,)),
        (n.PRED, (k.LPAREN, n.P, k.RPAREN)), (n.PRED, (k.NOT, n.R)),
        (n.R, (n.E, k.LT, n.E)), (n.R, (n.E, k.GT, n.E)),
        (n.R, (n.E, k.EQ, n.E)), (n.R, (n.E, k.MEMBER, n.E)),
        (n.C, (k.SHOW, n.A)), (n.A, (n.E,)), (n.A, (n.P,)),
    )
    names = SPELLINGS + ("S'", "S", "D'", "D", "T", "E", "E'", "E''", "Z", "P", "P'", "P''", "R", "C", "A")
    return Grammar(names, 26, 25, tuple(Production(int(lhs), tuple(map(int, rhs)))
                                       for lhs, rhs in definitions))
