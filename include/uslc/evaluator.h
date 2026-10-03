#ifndef USLC_EVALUATOR_H
#define USLC_EVALUATOR_H
#include "uslc/ast.h"
typedef enum {
    USLC_VALUE_INTEGER,
    USLC_VALUE_BOOLEAN,
    USLC_VALUE_VARIABLE,
    USLC_VALUE_SET,
    USLC_VALUE_BINARY,
    USLC_VALUE_NOT
} UslcValueKind;
/* Arena-owned immutable symbolic tree. Globally unique binder IDs prevent capture. */
typedef struct UslcValue {
    UslcValueKind kind;
    UslcType type;
    UslcTokenKind op;
    UslcSpan span;
    int64_t integer;
    bool boolean;
    size_t binding;
    const struct UslcValue *left, *right;
} UslcValue;
bool uslc_checked_add(int64_t a, int64_t b, int64_t *result);
bool uslc_checked_sub(int64_t a, int64_t b, int64_t *result);
bool uslc_checked_mul(int64_t a, int64_t b, int64_t *result);
bool uslc_evaluate(const UslcAst *program, size_t binding_count, UslcArena *arena,
                   const UslcValue **result, UslcDiagnostics *d);
#endif
