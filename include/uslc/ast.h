#ifndef USLC_AST_H
#define USLC_AST_H
#include "uslc/parser.h"
typedef enum {
    USLC_TYPE_VOID,
    USLC_TYPE_INTEGER,
    USLC_TYPE_BOOLEAN,
    USLC_TYPE_SET,
    USLC_TYPE_ERROR
} UslcType;
typedef enum {
    USLC_AST_PROGRAM,
    USLC_AST_DECLARATION,
    USLC_AST_SHOW,
    USLC_AST_INTEGER,
    USLC_AST_NAME,
    USLC_AST_SET,
    USLC_AST_ARITHMETIC,
    USLC_AST_SET_BINARY,
    USLC_AST_COMPARISON,
    USLC_AST_MEMBERSHIP,
    USLC_AST_LOGICAL,
    USLC_AST_NOT
} UslcAstKind;
/* Arena-owned tree. left/right are children; next links declarations in source order. */
typedef struct UslcAst {
    UslcAstKind kind;
    UslcType type, declared_type;
    UslcSpan span;
    UslcToken token;
    size_t binding;
    struct UslcAst *left, *right, *next;
} UslcAst;
const char *uslc_type_name(UslcType type);
const char *uslc_ast_kind_name(UslcAstKind kind);
bool uslc_ast_lower(const UslcParseNode *root, UslcArena *arena, UslcAst **ast, UslcDiagnostics *d);
#endif
