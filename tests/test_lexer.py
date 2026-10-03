import unittest

from uslc.diagnostic import Diagnostic, Position
from uslc.lexer import Lexer, lex
from uslc.source import Source
from uslc.token import INT64_MAX, Kind


class LexerTests(unittest.TestCase):
    def test_all_terminals(self):
        tokens = lex(Source.from_text(". let abc be int set U+-I*9(){}:|&!<>=@show"))
        self.assertEqual([token.kind for token in tokens], list(Kind))

    def test_longest_match(self):
        tokens = lex(Source.from_text("letbe be123+a true false"))
        self.assertEqual([t.kind for t in tokens],
                         [Kind.ID, Kind.BE, Kind.NUM, Kind.PLUS, Kind.ID, Kind.ID, Kind.ID, Kind.EOF])
        self.assertEqual(tokens[2].number, 123)

    def test_crlf_and_whitespace_spans(self):
        source = Source.from_text("letbe be123+a\r\n\tshow\r0\n\f\v.")
        tokens = lex(source)
        self.assertEqual(tokens[5].span.start, Position(2, 2, 16))
        self.assertEqual(tokens[6].span.start.line, 3)
        self.assertEqual(tokens[7].span.start.line, 4)
        self.assertEqual(tokens[7].span.start.column, 3)
        for left, right in zip(tokens, tokens[1:]):
            self.assertLessEqual(left.span.end.offset, right.span.start.offset)
        self.assertEqual(tokens[-1].span.start.offset, len(source.data))

    def test_integer_bounds_and_minus(self):
        tokens = lex(Source.from_text(f"{INT64_MAX} 0 -1"))
        self.assertEqual(tokens[0].number, INT64_MAX)
        self.assertEqual(tokens[2].kind, Kind.MINUS)
        self.assertEqual(tokens[3].number, 1)

    def test_lexical_errors(self):
        cases = [(b"012", "L1002", 3), (b"00", "L1002", 2),
                 (str(INT64_MAX + 1).encode(), "L1003", 19),
                 (b"9" * 10000, "L1003", 10000), (b"A", "L1001", 1),
                 (b"_", "L1001", 1), (b"/", "L1001", 1),
                 ("\u00e9".encode(), "L1001", 1), (b"\x00", "L1001", 1)]
        for data, code, end in cases:
            with self.subTest(data=data[:20]):
                with self.assertRaises(Diagnostic) as caught:
                    lex(Source("invalid", data))
                self.assertEqual(caught.exception.code, code)
                self.assertEqual(caught.exception.status, 1)
                self.assertEqual(caught.exception.span.end.offset, end)

    def test_empty_and_repeated_eof(self):
        lexer = Lexer(Source.from_text(""))
        self.assertEqual(lexer.next_token(), lexer.next_token())
        self.assertEqual(lex(Source.from_text(" \t\r\n"))[-1].kind, Kind.EOF)

    def test_all_byte_values(self):
        accepted = set(b"abcdefghijklmnopqrstuvwxyz0123456789UI.+-*@<>=|&!(){}: \t\r\n\f\v")
        for byte in range(256):
            with self.subTest(byte=byte):
                if byte in accepted:
                    self.assertEqual(lex(Source("byte", bytes([byte])))[-1].kind, Kind.EOF)
                else:
                    with self.assertRaises(Diagnostic) as caught:
                        lex(Source("byte", bytes([byte])))
                    self.assertEqual(caught.exception.code, "L1001")
