#include "uslc/token.h"
#include <string.h>
static const char *const names[] = {
    "DOT",          "LET",  "ID",  "BE",     "INT",    "SET",    "UNION",  "PLUS",  "MINUS",
    "INTERSECTION", "STAR", "NUM", "LPAREN", "RPAREN", "LBRACE", "RBRACE", "COLON", "OR",
    "AND",          "NOT",  "LT",  "GT",     "EQ",     "MEMBER", "SHOW",   "EOF",   "ERROR"};
static const char *const spellings[] = {
    ".", "let", "id", "be", "int", "set", "U", "+", "-", "I", "*",    "num", "(",    ")",
    "{", "}",   ":",  "|",  "&",   "!",   "<", ">", "=", "@", "show", "$",   "error"};
const char *uslc_token_name(UslcTokenKind kind) {
    return names[kind];
}
const char *uslc_token_spelling(UslcTokenKind kind) {
    return spellings[kind];
}
bool uslc_token_equals(const UslcSource *s, UslcToken a, UslcToken b) {
    size_t n = a.span.end.offset - a.span.start.offset;
    return n == b.span.end.offset - b.span.start.offset &&
           !memcmp(s->data + a.span.start.offset, s->data + b.span.start.offset, n);
}
