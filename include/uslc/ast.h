#ifndef USLC_AST_H
#define USLC_AST_H
#include <stdint.h>
#include "uslc/diagnostic.h"
typedef struct UslcParseContext UslcParseContext;
typedef struct UslcParseNode UslcParseNode;
typedef struct UslcValue UslcValue;
typedef enum { USLC_VOID, USLC_INTEGER, USLC_BOOLEAN, USLC_SET, USLC_ERROR } UslcType;
typedef enum { USLC_PROGRAM, USLC_DECLARATION, USLC_SHOW, USLC_INT_LITERAL,
    USLC_NAME, USLC_SET_LITERAL, USLC_ARITHMETIC, USLC_SET_BINARY,
    USLC_COMPARISON, USLC_MEMBERSHIP, USLC_LOGICAL, USLC_NOT } UslcAstKind;
/* Arena-owned node; all pointers borrow from the compilation context. */
typedef struct UslcAst {
    UslcAstKind kind;
    UslcType type, declared_type;
    UslcSpan span;
    const UslcParseNode *token;
    int op;
    int64_t integer;
    size_t binding, height;
    struct UslcAst *left, *right, *next;
    const UslcValue *value;
} UslcAst;
bool uslc_lower(UslcParseContext *context);
const char *uslc_type_name(UslcType type);
const char *uslc_ast_kind_name(UslcAstKind kind);
#endif
