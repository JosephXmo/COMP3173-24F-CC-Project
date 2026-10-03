#ifndef USLC_PARSER_H
#define USLC_PARSER_H
#include "uslc/arena.h"
#include "uslc/lexer.h"
#include "uslc/slr.h"
/* Arena-owned node; terminal tokens borrow source. Production SIZE_MAX denotes a terminal. */
typedef struct UslcParseNode {
    size_t symbol, production, count, height;
    UslcSpan span;
    UslcToken token;
    struct UslcParseNode *children[USLC_MAX_RHS];
} UslcParseNode;
typedef void (*UslcTrace)(void *context, size_t state, UslcToken token, UslcAction action);
bool uslc_parse(const UslcSlr *slr, const UslcTokens *tokens, UslcArena *arena,
                UslcParseNode **root, size_t *node_count, UslcDiagnostics *d, UslcTrace trace,
                void *context);
#endif
