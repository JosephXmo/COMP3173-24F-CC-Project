from dataclasses import replace
import random
import unittest

from uslc.ast import NodeKind, Type, lower_ast
from uslc.diagnostic import Diagnostic
from uslc.evaluator import checked_arithmetic, evaluate
from uslc.token import INT64_MAX, INT64_MIN, Kind
from .common import compile_text


class SemanticsTests(unittest.TestCase):
    def test_precedence_and_left_associativity(self):
        result = compile_text("show 9-3-2*2.")
        self.assertEqual(result.status, 0)
        self.assertEqual(result.display, "2")
        node = result.ast.right.left
        self.assertEqual(node.token.kind, Kind.MINUS)
        self.assertEqual(node.left.token.kind, Kind.MINUS)
        self.assertEqual(node.right.token.kind, Kind.STAR)
        self.assertEqual(result.parsed.root.children[1].token.kind, Kind.DOT)

    def test_parser_errors(self):
        cases = ["", "show 1", "show 1..", "show 1.show 2.", "show -1.",
                 "show !(1<2).", "show !!1<2.", "let int x be 1.", "show 1<2<3."]
        for text in cases:
            with self.subTest(text=text):
                result = compile_text(text)
                self.assertEqual(result.status, 2)
                self.assertIn("expected:", result.diagnostic.message)

    def test_malformed_tree(self):
        root = compile_text("show 1.", phase="parse").parsed.root
        with self.assertRaises(Diagnostic) as caught:
            lower_ast(replace(root, children=()))
        self.assertEqual(caught.exception.code, "I9003")

    def test_depth_limit(self):
        result = compile_text("show " + "(" * 300 + "1" + ")" * 300 + ".")
        self.assertEqual(result.status, 70)
        self.assertEqual(result.diagnostic.code, "I9004")

    def test_declaration_errors(self):
        cases = [("show x.", "T3001"), ("let int x be x.show x.", "T3001"),
                 ("let int x be 1.let int x be 2.show x.", "T3002"),
                 ("let set x be 4.show x.", "T3004"), ("let int x be {y:y>0}.show x.", "T3004"),
                 ("let set s be {x:x>0}.show x.", "T3001"), ("show true.", "T3001")]
        for text, code in cases:
            with self.subTest(text=text):
                result = compile_text(text)
                self.assertEqual(result.status, 3)
                self.assertEqual(result.diagnostic.code, code)

    def test_every_binary_operand_signature(self):
        operands = ("1", "{x:x>0}")
        for operator in ("+", "-", "*", "U", "I", "<", ">", "=", "@"):
            for left in range(2):
                for right in range(2):
                    valid = ((left, right) == (1, 1) if operator in ("U", "I")
                             else (left, right) == (0, 1) if operator == "@"
                             else (left, right) == (0, 0))
                    text = f"show {operands[left]}{operator}{operands[right]}."
                    with self.subTest(text=text):
                        result = compile_text(text)
                        self.assertEqual(result.status, 0 if valid else 3)
                        if not valid:
                            self.assertEqual(result.diagnostic.code, "T3003")

    def test_binding_and_shadowing(self):
        result = compile_text("let set x be {x:x>0}.show {x:x@{x:x>0}}.", phase="check")
        self.assertEqual(result.status, 0)
        outer = result.ast.right.left
        inner = outer.left.right
        self.assertNotEqual(outer.binding, inner.binding)
        self.assertEqual(outer.left.left.binding, outer.binding)
        self.assertEqual(inner.left.left.binding, inner.binding)
        self.assertEqual(result.symbols[0].type, Type.SET)

    def test_int64_boundaries(self):
        valid = [(Kind.PLUS, INT64_MIN, INT64_MAX, -1), (Kind.MINUS, INT64_MIN, INT64_MIN, 0),
                 (Kind.STAR, INT64_MIN, 1, INT64_MIN), (Kind.STAR, INT64_MAX, 0, 0)]
        invalid = [(Kind.PLUS, INT64_MAX, 1), (Kind.PLUS, INT64_MIN, -1),
                   (Kind.MINUS, INT64_MIN, 1), (Kind.MINUS, INT64_MAX, -1),
                   (Kind.STAR, INT64_MIN, -1), (Kind.STAR, -1, INT64_MIN),
                   (Kind.STAR, INT64_MAX, 2), (Kind.STAR, -(1 << 62), -2)]
        for op, left, right, expected in valid:
            self.assertEqual(checked_arithmetic(op, left, right), expected)
        for op, left, right in invalid:
            with self.assertRaises(Diagnostic) as caught:
                checked_arithmetic(op, left, right)
            self.assertEqual(caught.exception.code, "E4001")
        self.assertEqual(compile_text(f"show 0-{INT64_MAX}-1.").value.integer, INT64_MIN)

    def test_short_circuit_and_static_check(self):
        cases = [(f"show 1<2|{INT64_MAX}+1>0.", "true"),
                 (f"show 1>2&{INT64_MAX}+1>0.", "false"),
                 (f"show 2@{{x:x>0|{INT64_MAX}+1>0}}.", "true"),
                 ("show !1=2.", "true")]
        for text, expected in cases:
            self.assertEqual(compile_text(text).display, expected)
        self.assertEqual(compile_text(f"show 1@{{x:x>2|{INT64_MAX}+1>0}}.").status, 4)
        self.assertEqual(compile_text("show 1<2|1@2.").status, 3)

    def test_captured_sets(self):
        cases = [("show 3@({x:x>3}U{y:y<1}).", "false"),
                 ("show 4@({x:x>3}I{y:y<5}).", "true"),
                 ("let int x be 3.let set s be {x:x>1}.show x@s.", "true"),
                 ("show 3@{x:4@{y:y>x}}.", "true"), ("show 3@{x:2@{x:x>1}}.", "true"),
                 ("show 3@({x:4@{y:y>x}}I{y:y<4}).", "true"),
                 ("show 3@{x:4@({y:y>x}U{z:z<0})}.", "true"),
                 ("let set s be {x:x>0}.let set t be s I {x:x<5}.show 2@t.", "true"),
                 ("show {a:a>3}U{b:b>5}.", "{ x : ((x > 3) | (x > 5)) }"),
                 ("show {a:2@{b:b>a}}U{b:b<1}.", "{ x : ((2 @ { xa : (xa > x) }) | (x < 1)) }")]
        for text, expected in cases:
            with self.subTest(text=text):
                result = compile_text(text)
                self.assertEqual(result.status, 0, result.report())
                self.assertEqual(result.display, expected)

    def test_evaluator_invariant(self):
        result = compile_text("show {x:x>1}.", phase="check")
        with self.assertRaises(Diagnostic) as caught:
            evaluate(result.ast, 0)
        self.assertEqual(caught.exception.code, "E4090")

    def test_seeded_arithmetic_properties(self):
        rng = random.Random(3173)
        for _ in range(100):
            a, b, c, d = (rng.randrange(1000) for _ in range(4))
            result = compile_text(f"show ({a}-{b})*{c}+{d}.")
            self.assertEqual(result.status, 0)
            self.assertEqual(result.value.integer, (a - b) * c + d)

    def test_seeded_set_properties(self):
        rng = random.Random(241)
        for _ in range(100):
            n, low, high, special = (rng.randrange(20) for _ in range(4))
            source = f"show {n}@({{x:x>{low}}}I{{y:y<{high}}}U{{z:z={special}}})."
            result = compile_text(source)
            self.assertEqual(result.status, 0)
            self.assertEqual(result.value.boolean, (n > low and n < high) or n == special)
