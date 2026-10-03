import csv
from dataclasses import replace
import unittest

from uslc.diagnostic import Diagnostic, Position, Span
from uslc.grammar import Grammar, Nonterminal, Production, analyze, core_grammar
from uslc.parser import parse
from uslc.slr import build_slr, closure, goto
from uslc.token import Kind, Token
from .common import FIXTURES, TABLE


class GrammarTests(unittest.TestCase):
    def test_core_first_follow(self):
        self.assertEqual(len(TABLE.grammar.productions), 34)
        self.assertEqual(TABLE.analysis.first[Nonterminal.S], {Kind.LET, Kind.SHOW})
        self.assertEqual(TABLE.analysis.follow[Nonterminal.S], {Kind.EOF})
        self.assertFalse(any(TABLE.analysis.nullable))

    def test_nullable_recursive_grammar(self):
        grammar = Grammar(("a", "b", "$", "S'", "S", "A", "B"), 3, 2,
                          (Production(3, (4,)), Production(4, (5, 6)),
                           Production(5, (0, 5)), Production(5, ()),
                           Production(6, (1, 6)), Production(6, ())))
        result = analyze(grammar)
        self.assertTrue(all(result.nullable[3:]))
        self.assertEqual(result.first[4], {0, 1})
        self.assertEqual(result.follow[5], {1, 2})
        self.assertEqual(result.follow[6], {2})
        self.assertEqual(build_slr(grammar), build_slr(grammar, workers=4))

    def test_epsilon_reduction(self):
        grammar = Grammar(("$", "S'", "S"), 1, 0, (Production(1, (2,)), Production(2, ())))
        span = Span(Position(1, 1, 0), Position(1, 1, 0))
        result = parse(build_slr(grammar), (Token(Kind.DOT, span),))
        self.assertEqual(result.node_count, 1)
        self.assertEqual(result.root.production, 1)
        self.assertEqual(result.root.children, ())
        self.assertEqual(result.root.span, span)

    def test_invalid_grammar(self):
        grammar = core_grammar()
        cases = [replace(grammar, terminals=0), replace(grammar, eof=99),
                 replace(grammar, productions=()), replace(grammar, names=()),
                 replace(grammar, productions=(Production(26, (0,)),)),
                 replace(grammar, productions=grammar.productions + (Production(26, (27,)),)),
                 replace(grammar, productions=(Production(26, (27,)), Production(27, (99,))))]
        for invalid in cases:
            with self.subTest(grammar=invalid):
                with self.assertRaises(Diagnostic) as caught:
                    analyze(invalid)
                self.assertEqual(caught.exception.code, "I9002")

    def test_closure_goto_and_unique_states(self):
        self.assertEqual(len(TABLE.states), 66)
        self.assertEqual(len({state.items for state in TABLE.states}), 66)
        initial = closure(TABLE.grammar, frozenset({(0, 0)}))
        self.assertEqual(initial, TABLE.states[0].items)
        self.assertTrue({(0, 0), (5, 0), (31, 0)} <= initial)
        self.assertEqual(goto(TABLE.grammar, initial, Nonterminal.S), {(0, 1)})

    def test_parallel_table_is_identical(self):
        for workers in (2, 4, 8):
            with self.subTest(workers=workers):
                self.assertEqual(build_slr(core_grammar(), workers), TABLE)

    def test_conflicts(self):
        ambiguous = Grammar(("a", "+", "$", "S'", "S"), 3, 2,
                            (Production(3, (4,)), Production(4, (4, 1, 4)), Production(4, (0,))))
        reduce_reduce = Grammar(("a", "$", "S'", "S", "A", "B"), 2, 1,
                                (Production(2, (3,)), Production(3, (4,)), Production(3, (5,)),
                                 Production(4, (0,)), Production(5, (0,))))
        for grammar in (ambiguous, reduce_reduce):
            messages = []
            for workers in (1, 4):
                with self.assertRaises(Diagnostic) as caught:
                    build_slr(grammar, workers)
                self.assertEqual(caught.exception.code, "P2090")
                messages.append(caught.exception.message)
            self.assertEqual(messages[0], messages[1])

    def test_full_course_table_isomorphism(self):
        with (FIXTURES / "slr-reference.csv").open(newline="", encoding="utf-8") as file:
            rows = list(csv.reader(file))[2:]
        mapping = {0: 0}
        pending = [0]
        while pending:
            original = pending.pop(0)
            state = TABLE.states[mapping[original]]
            for symbol, cell in enumerate(rows[original][1:42]):
                if not cell or (symbol < 26 and not cell.startswith("s")):
                    continue
                target = int(cell[1:] if symbol < 26 else cell)
                actual = state.transitions[symbol]
                self.assertIsNotNone(actual)
                if target not in mapping:
                    self.assertNotIn(actual, mapping.values())
                    mapping[target] = actual
                    pending.append(target)
                self.assertEqual(mapping[target], actual)
        self.assertEqual(len(mapping), 66)
        for original, row in enumerate(rows):
            state = TABLE.states[mapping[original]]
            for symbol, cell in enumerate(row[1:42]):
                if symbol >= 26:
                    self.assertEqual(state.transitions[symbol], mapping[int(cell)] if cell else None)
                else:
                    action = state.actions[symbol]
                    expected = ("error", 0) if not cell else ("accept", 0) if cell == "acc" else (
                        ("shift", mapping[int(cell[1:])]) if cell[0] == "s" else ("reduce", int(cell[1:])))
                    self.assertEqual((action.kind, action.target), expected)
