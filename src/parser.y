%define api.pure full
%define api.prefix {uslc_}
%define parse.error detailed
%define parse.lac full
%define parse.trace
%define lr.type lalr
%define api.location.type {UslcSpan}
%locations
%expect 0
%parse-param { UslcParseContext *context }
%parse-param { yyscan_t scanner }
%lex-param { yyscan_t scanner }
%code requires {
#include <stdint.h>
#include "uslc/diagnostic.h"
#include "uslc/string.h"
typedef void *yyscan_t;
typedef struct UslcParseContext UslcParseContext;
typedef struct UslcParseNode UslcParseNode;
#ifndef YYLLOC_DEFAULT
#define YYLLOC_DEFAULT(Current, Rhs, N) do { \
    (Current).start=(N) ? YYRHSLOC(Rhs,1).start : YYRHSLOC(Rhs,0).end; \
    (Current).end=YYRHSLOC(Rhs,N).end; \
} while (0)
#endif
}
%code {
#include <stdio.h>
#include <string.h>
#include "uslc/parse_context.h"
#define YYLOCATION_PRINT(file, loc) fprintf(file, "%zu:%zu-%zu:%zu", \
    (loc)->start.line, (loc)->start.column, (loc)->end.line, (loc)->end.column)
int uslc_lex(USLC_STYPE *, USLC_LTYPE *, yyscan_t);
static void uslc_error(UslcSpan *location, UslcParseContext *context, yyscan_t scanner, const char *message) {
    (void)scanner;
    if (context->diagnostics.status) return;
    if (!strcmp(message, "memory exhausted")) {
        uslc_diagnose(&context->diagnostics, USLC_INTERNAL, "I9001", *location, "parser memory exhausted");
        return;
    }
    bool eof=location->start.offset==context->source.length;
    uslc_diagnose(&context->diagnostics, USLC_SYNTAX, eof ? "P2002" : "P2001",
                  *location, eof ? "unexpected end of input" : "unexpected token");
    snprintf(context->diagnostics.note, sizeof context->diagnostics.note, "%s", message);
}
#define REDUCE(dst, symbol, id, span, count, ...) do { \
    (dst)=uslc_reduce(context, symbol, id, span, count, __VA_ARGS__); \
    if (!(dst)) YYABORT; \
} while (0)
}
%union { int64_t integer; UslcString identifier; UslcParseNode *node; }
%token <node> LET BE INT SET SHOW ID NUM
%token <node> PLUS MINUS STAR LT GT EQ MEMBER AND OR NOT UNION INTERSECTION
%token <node> DOT LPAREN RPAREN LBRACE RBRACE COLON
%type <node> program declarations declaration type expression term factor representative
%type <node> predicate conjunction predicate_factor relation command argument
/* All semantic nodes already belong to context->parse_arena. */
%destructor { (void)$$; } <node>
%start program
%%
program:
    declarations command DOT { REDUCE($$, "S", 1, @$, 3, $1,$2,$3); context->root=$$; }
  | command DOT { REDUCE($$, "S", 2, @$, 2, $1,$2); context->root=$$; }
;
declarations:
    declaration declarations { REDUCE($$, "D'", 3, @$, 2, $1,$2); }
  | declaration { REDUCE($$, "D'", 4, @$, 1, $1); }
;
declaration: LET type ID BE expression DOT { REDUCE($$, "D", 5, @$, 6, $1,$2,$3,$4,$5,$6); };
type:
    INT { REDUCE($$, "T", 6, @$, 1, $1); }
  | SET { REDUCE($$, "T", 7, @$, 1, $1); }
;
expression:
    term { REDUCE($$, "E", 8, @$, 1, $1); }
  | expression UNION term { REDUCE($$, "E", 9, @$, 3, $1,$2,$3); }
  | expression PLUS term { REDUCE($$, "E", 10, @$, 3, $1,$2,$3); }
  | expression MINUS term { REDUCE($$, "E", 11, @$, 3, $1,$2,$3); }
;
term:
    factor { REDUCE($$, "E'", 12, @$, 1, $1); }
  | term INTERSECTION factor { REDUCE($$, "E'", 13, @$, 3, $1,$2,$3); }
  | term STAR factor { REDUCE($$, "E'", 14, @$, 3, $1,$2,$3); }
;
factor:
    NUM { REDUCE($$, "E''", 15, @$, 1, $1); }
  | ID { REDUCE($$, "E''", 16, @$, 1, $1); }
  | LPAREN expression RPAREN { REDUCE($$, "E''", 17, @$, 3, $1,$2,$3); }
  | LBRACE representative predicate RBRACE { REDUCE($$, "E''", 18, @$, 4, $1,$2,$3,$4); }
;
representative: ID COLON { REDUCE($$, "Z", 19, @$, 2, $1,$2); };
predicate:
    predicate OR conjunction { REDUCE($$, "P", 20, @$, 3, $1,$2,$3); }
  | conjunction { REDUCE($$, "P", 21, @$, 1, $1); }
;
conjunction:
    conjunction AND predicate_factor { REDUCE($$, "P'", 22, @$, 3, $1,$2,$3); }
  | predicate_factor { REDUCE($$, "P'", 23, @$, 1, $1); }
;
predicate_factor:
    relation { REDUCE($$, "P''", 24, @$, 1, $1); }
  | LPAREN predicate RPAREN { REDUCE($$, "P''", 25, @$, 3, $1,$2,$3); }
  | NOT relation { REDUCE($$, "P''", 26, @$, 2, $1,$2); }
;
relation:
    expression LT expression { REDUCE($$, "R", 27, @$, 3, $1,$2,$3); }
  | expression GT expression { REDUCE($$, "R", 28, @$, 3, $1,$2,$3); }
  | expression EQ expression { REDUCE($$, "R", 29, @$, 3, $1,$2,$3); }
  | expression MEMBER expression { REDUCE($$, "R", 30, @$, 3, $1,$2,$3); }
;
command: SHOW argument { REDUCE($$, "C", 31, @$, 2, $1,$2); };
argument:
    expression { REDUCE($$, "A", 32, @$, 1, $1); }
  | predicate { REDUCE($$, "A", 33, @$, 1, $1); }
;
%%
