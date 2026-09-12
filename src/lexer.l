%option reentrant bison-bridge bison-locations
%option noyywrap nounput noinput nodefault
%option noyyalloc noyyrealloc noyyfree
%option prefix="uslc_"
%option extra-type="UslcScanner *"
%{
#include "uslc/parse_context.h"
#include "parser.h"
#define YYSTYPE USLC_STYPE
#define YYLTYPE USLC_LTYPE
#define YY_USER_ACTION uslc_scanner_location(yyextra, yylloc, yytext, (size_t)yyleng);
#define YY_FATAL_ERROR(message) uslc_scanner_fatal(uslc_get_extra(yyscanner), message)
#define TOKEN(kind) return uslc_scanner_token(yyextra, &yylval->node, *yylloc, kind, #kind)
%}
%%
[ \t\r\n\f\v]+  ;
let                 { TOKEN(LET); }
be                  { TOKEN(BE); }
int                 { TOKEN(INT); }
set                 { TOKEN(SET); }
show                { TOKEN(SHOW); }
[a-z]+              { TOKEN(ID); }
0|[1-9][0-9]*       { TOKEN(NUM); }
[0-9]+              { uslc_diagnose(&yyextra->context->diagnostics, USLC_LEXICAL, "L1002", *yylloc, "leading zero in integer literal"); return USLC_UNDEF; }
U                   { TOKEN(UNION); }
I                   { TOKEN(INTERSECTION); }
"+"                 { TOKEN(PLUS); }
"-"                 { TOKEN(MINUS); }
"*"                 { TOKEN(STAR); }
"<"                 { TOKEN(LT); }
">"                 { TOKEN(GT); }
"="                 { TOKEN(EQ); }
"@"                 { TOKEN(MEMBER); }
"&"                 { TOKEN(AND); }
"|"                 { TOKEN(OR); }
"!"                 { TOKEN(NOT); }
"."                 { TOKEN(DOT); }
"("                 { TOKEN(LPAREN); }
")"                 { TOKEN(RPAREN); }
"{"                 { TOKEN(LBRACE); }
"}"                 { TOKEN(RBRACE); }
":"                 { TOKEN(COLON); }
<<EOF>>             { *yylloc=(UslcSpan){yyextra->position, yyextra->position}; return uslc_scanner_token(yyextra, &yylval->node, *yylloc, 0, "EOF"); }
.                   { uslc_diagnose(&yyextra->context->diagnostics, USLC_LEXICAL, "L1001", *yylloc, "invalid character"); return USLC_UNDEF; }
%%
