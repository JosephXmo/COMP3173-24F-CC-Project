#ifndef USLC_PARSE_CONTEXT_H
#define USLC_PARSE_CONTEXT_H
#include <setjmp.h>
#include <stdint.h>
#include "uslc/arena.h"
#include "uslc/source.h"
#include "uslc/string.h"
#include "uslc/symbol_table.h"
#define USLC_DEPTH_LIMIT 255
typedef void *yyscan_t;
/* Nodes and token array belong to the context; lexemes borrow source bytes. */
struct UslcParseNode {
    const char *symbol;
    int production_id, token_kind;
    UslcSpan span;
    int64_t integer;
    size_t child_count, height;
    UslcParseNode *children[6];
};
/* Owns source, three arenas, tokens, and symbols. Zero/init before use;
   free is valid at every stage, including partially completed compilation. */
struct UslcParseContext {
    UslcSource source;
    UslcDiagnostics diagnostics;
    UslcArena parse_arena, ast_arena, value_arena;
    UslcParseNode **tokens, *root;
    size_t token_count, token_capacity, parse_node_count, next_binding;
    UslcAst *ast;
    UslcSymbolTable symbols;
    const UslcValue *result;
    bool lexed, parsed, lowered, checked, evaluated;
};
/* Owns Flex scanner and buffer. Context and source are borrowed. */
typedef struct {
    yyscan_t handle;
    void *buffer;
    struct UslcScanAllocation *allocations;
    UslcParseContext *context;
    UslcPosition position;
    bool previous_cr, replay, eof;
    size_t index;
    jmp_buf failure;
} UslcScanner;
void uslc_context_init(UslcParseContext *context);
#ifdef USLC_TESTING
void uslc_scanner_test_fail_after(size_t successful_allocations);
#endif
void uslc_context_free(UslcParseContext *context);
bool uslc_scanner_init(UslcScanner *scanner, UslcParseContext *context, bool replay);
void uslc_scanner_destroy(UslcScanner *scanner);
int uslc_scanner_next(UslcScanner *scanner, UslcParseNode **node);
void uslc_scanner_location(UslcScanner *scanner, UslcSpan *span, const char *text, size_t length);
int uslc_scanner_token(UslcScanner *scanner, UslcParseNode **node, UslcSpan span,
                       int kind, const char *symbol);
void uslc_scanner_fatal(UslcScanner *scanner, const char *message);
bool uslc_scan(UslcParseContext *context);
bool uslc_parse_source(UslcParseContext *context, bool trace);
UslcParseNode *uslc_reduce(UslcParseContext *context, const char *symbol,
                          int production, UslcSpan span, size_t count, ...);
const char *uslc_operator_name(int op);
#endif
