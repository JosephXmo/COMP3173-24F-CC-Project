#ifndef USLC_EVALUATOR_H
#define USLC_EVALUATOR_H
#include "uslc/ast.h"
typedef enum { USLC_VALUE_INT, USLC_VALUE_BOOL, USLC_VALUE_VAR,
    USLC_VALUE_SET, USLC_VALUE_BINARY, USLC_VALUE_NOT } UslcValueKind;
/* Immutable symbolic DAG, owned by the context's value arena. Binding IDs
   distinguish representatives even when source spellings are identical. */
struct UslcValue {
    UslcValueKind kind;
    UslcType type;
    UslcSpan span;
    int op;
    int64_t integer;
    bool boolean;
    size_t binding, height;
    const UslcValue *left, *right;
};
bool uslc_evaluate(UslcParseContext *context);
bool uslc_checked_add(int64_t a, int64_t b, int64_t *result);
bool uslc_checked_sub(int64_t a, int64_t b, int64_t *result);
bool uslc_checked_mul(int64_t a, int64_t b, int64_t *result);
#endif
