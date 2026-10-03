#ifndef USLC_LEXER_H
#define USLC_LEXER_H
#include "uslc/token.h"
/* Borrows source and diagnostics. CRLF is one logical line break. */
typedef struct {
    const UslcSource *source;
    UslcDiagnostics *diagnostics;
    UslcPosition position;
    bool previous_cr;
} UslcLexer;
/* Owns the token array, whose slices borrow the source. */
typedef struct {
    UslcToken *data;
    size_t count, capacity;
} UslcTokens;
void uslc_tokens_init(UslcTokens *tokens);
void uslc_lexer_init(UslcLexer *l, const UslcSource *s, UslcDiagnostics *d);
UslcToken uslc_lexer_next(UslcLexer *l);
bool uslc_lex(UslcTokens *tokens, const UslcSource *s, UslcDiagnostics *d);
void uslc_tokens_free(UslcTokens *tokens);
#endif
