#include "uslc/evaluator.h"
#include <stdlib.h>
bool uslc_checked_add(int64_t a, int64_t b, int64_t *result) {
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
        return false;
    *result = a + b;
    return true;
}
bool uslc_checked_sub(int64_t a, int64_t b, int64_t *result) {
    if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b))
        return false;
    *result = a - b;
    return true;
}
bool uslc_checked_mul(int64_t a, int64_t b, int64_t *result) {
    if (a > 0) {
        if ((b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a))
            return false;
    } else if (a < 0) {
        if ((b > 0 && a < INT64_MIN / b) || (b < 0 && a < INT64_MAX / b))
            return false;
    }
    *result = a * b;
    return true;
}
typedef struct {
    UslcArena *arena;
    UslcDiagnostics *d;
    const UslcValue **environment;
    size_t count, next_binding;
} Evaluator;
static UslcValue *value(Evaluator *e, UslcValueKind kind, UslcType type, UslcSpan span) {
    UslcValue *v = uslc_arena_alloc(e->arena, sizeof *v, e->d);
    if (v) {
        v->kind = kind;
        v->type = type;
        v->span = span;
    }
    return v;
}
/* Closing captures immutable globals but deliberately defers predicate arithmetic. */
static const UslcValue *close_ast(Evaluator *e, const UslcAst *a, size_t depth) {
    if (!a || a->type == USLC_TYPE_VOID || a->type == USLC_TYPE_ERROR ||
        a->kind < USLC_AST_INTEGER || a->kind > USLC_AST_NOT) {
        uslc_diagnose(e->d, 70, "E4090", "internal", a ? a->span : (UslcSpan){0},
                      "expected a type-checked expression");
        return NULL;
    }
    if (!uslc_depth(e->d, depth, a->span))
        return NULL;
    if (a->kind == USLC_AST_NAME) {
        if (a->binding >= e->count || !e->environment[a->binding]) {
            uslc_diagnose(e->d, 70, "E4090", "internal", a->span,
                          "unbound identifier after type checking");
            return NULL;
        }
        return e->environment[a->binding];
    }
    UslcValueKind kind = a->kind == USLC_AST_INTEGER ? USLC_VALUE_INTEGER
                         : a->kind == USLC_AST_SET   ? USLC_VALUE_SET
                         : a->kind == USLC_AST_NOT   ? USLC_VALUE_NOT
                                                     : USLC_VALUE_BINARY;
    UslcValue *v = value(e, kind, a->type, a->token.span);
    if (!v)
        return NULL;
    v->op = a->token.kind;
    if (kind == USLC_VALUE_INTEGER) {
        v->integer = a->token.number;
        return v;
    }
    if (kind == USLC_VALUE_SET) {
        if (a->binding >= e->count) {
            uslc_diagnose(e->d, 70, "E4090", "internal", a->span, "invalid representative binding");
            return NULL;
        }
        v->span = a->span;
        v->binding = a->binding;
        UslcValue *variable = value(e, USLC_VALUE_VARIABLE, USLC_TYPE_INTEGER, a->token.span);
        if (!variable)
            return NULL;
        variable->binding = a->binding;
        const UslcValue *saved = e->environment[a->binding];
        e->environment[a->binding] = variable;
        v->left = close_ast(e, a->left, depth + 1);
        e->environment[a->binding] = saved;
    } else {
        v->left = close_ast(e, a->left, depth + 1);
        if (v->left && a->right)
            v->right = close_ast(e, a->right, depth + 1);
    }
    return e->d->exit_code ? NULL : v;
}
static const UslcValue *substitute(Evaluator *e, const UslcValue *v, size_t binding,
                                   const UslcValue *replacement, size_t depth) {
    if (!uslc_depth(e->d, depth, v->span))
        return NULL;
    if (v->kind == USLC_VALUE_VARIABLE && v->binding == binding)
        return replacement;
    if (!v->left && !v->right)
        return v;
    const UslcValue *left =
        v->left ? substitute(e, v->left, binding, replacement, depth + 1) : NULL;
    if (e->d->exit_code)
        return NULL;
    const UslcValue *right =
        v->right ? substitute(e, v->right, binding, replacement, depth + 1) : NULL;
    if (e->d->exit_code)
        return NULL;
    if (left == v->left && right == v->right)
        return v;
    UslcValue *copy = value(e, v->kind, v->type, v->span);
    if (!copy)
        return NULL;
    *copy = *v;
    copy->left = left;
    copy->right = right;
    return copy;
}
static const UslcValue *evaluate(Evaluator *e, const UslcValue *v, size_t depth) {
    if (!uslc_depth(e->d, depth, v->span))
        return NULL;
    if (v->kind == USLC_VALUE_INTEGER || v->kind == USLC_VALUE_BOOLEAN || v->kind == USLC_VALUE_SET)
        return v;
    if (v->kind == USLC_VALUE_VARIABLE) {
        uslc_diagnose(e->d, 70, "E4002", "internal", v->span, "unbound set representative");
        return NULL;
    }
    if (!v->left || (v->kind == USLC_VALUE_BINARY && !v->right)) {
        uslc_diagnose(e->d, 70, "E4090", "internal", v->span, "missing evaluator operand");
        return NULL;
    }
    const UslcValue *left = evaluate(e, v->left, depth + 1);
    if (!left)
        return NULL;
    UslcType expected_left = USLC_TYPE_INTEGER, expected_right = USLC_TYPE_INTEGER;
    if (v->op == USLC_AND || v->op == USLC_OR || v->op == USLC_NOT)
        expected_left = expected_right = USLC_TYPE_BOOLEAN;
    else if (v->op == USLC_UNION || v->op == USLC_INTERSECTION)
        expected_left = expected_right = USLC_TYPE_SET;
    else if (v->op == USLC_MEMBER)
        expected_right = USLC_TYPE_SET;
    if (left->type != expected_left) {
        uslc_diagnose(e->d, 70, "E4090", "internal", v->span,
                      "invalid left operand after type checking");
        return NULL;
    }
    if ((v->op == USLC_AND && !left->boolean) || (v->op == USLC_OR && left->boolean))
        return left;
    const UslcValue *right = v->right ? evaluate(e, v->right, depth + 1) : NULL;
    if (e->d->exit_code)
        return NULL;
    if (right && right->type != expected_right) {
        uslc_diagnose(e->d, 70, "E4090", "internal", v->span,
                      "invalid right operand after type checking");
        return NULL;
    }
    if (v->op == USLC_MEMBER) {
        const UslcValue *predicate = substitute(e, right->left, right->binding, left, depth + 1);
        return predicate ? evaluate(e, predicate, depth + 1) : NULL;
    }
    if (v->op == USLC_UNION || v->op == USLC_INTERSECTION) {
        if (e->next_binding == SIZE_MAX) {
            uslc_oom(e->d);
            return NULL;
        }
        UslcValue *variable = value(e, USLC_VALUE_VARIABLE, USLC_TYPE_INTEGER, v->span);
        UslcValue *predicate = value(e, USLC_VALUE_BINARY, USLC_TYPE_BOOLEAN, v->span);
        UslcValue *set = value(e, USLC_VALUE_SET, USLC_TYPE_SET, v->span);
        if (!variable || !predicate || !set)
            return NULL;
        variable->binding = e->next_binding++;
        set->binding = variable->binding;
        predicate->op = v->op == USLC_UNION ? USLC_OR : USLC_AND;
        predicate->left = substitute(e, left->left, left->binding, variable, depth + 1);
        if (!predicate->left)
            return NULL;
        predicate->right = substitute(e, right->left, right->binding, variable, depth + 1);
        if (!predicate->right)
            return NULL;
        set->left = predicate;
        return set;
    }
    UslcValue *result =
        value(e, v->type == USLC_TYPE_INTEGER ? USLC_VALUE_INTEGER : USLC_VALUE_BOOLEAN, v->type,
              v->span);
    if (!result)
        return NULL;
    bool ok = true;
    switch (v->op) {
    case USLC_PLUS:
        ok = uslc_checked_add(left->integer, right->integer, &result->integer);
        break;
    case USLC_MINUS:
        ok = uslc_checked_sub(left->integer, right->integer, &result->integer);
        break;
    case USLC_STAR:
        ok = uslc_checked_mul(left->integer, right->integer, &result->integer);
        break;
    case USLC_LT:
        result->boolean = left->integer < right->integer;
        break;
    case USLC_GT:
        result->boolean = left->integer > right->integer;
        break;
    case USLC_EQ:
        result->boolean = left->integer == right->integer;
        break;
    case USLC_AND:
        result->boolean = left->boolean && right->boolean;
        break;
    case USLC_OR:
        result->boolean = left->boolean || right->boolean;
        break;
    case USLC_NOT:
        result->boolean = !left->boolean;
        break;
    default:
        uslc_diagnose(e->d, 70, "E4090", "internal", v->span, "invalid evaluator operator");
        return NULL;
    }
    if (!ok) {
        uslc_diagnose(e->d, 4, "E4001", "evaluate", v->span, "integer overflow in operator '%s'",
                      uslc_token_spelling(v->op));
        return NULL;
    }
    return result;
}
bool uslc_evaluate(const UslcAst *program, size_t binding_count, UslcArena *arena,
                   const UslcValue **result, UslcDiagnostics *d) {
    *result = NULL;
    if (!program || program->kind != USLC_AST_PROGRAM || !program->right ||
        program->right->kind != USLC_AST_SHOW)
        return uslc_diagnose(d, 70, "E4090", "internal", (UslcSpan){0},
                             "expected a program with one show command");
    Evaluator e = {.arena = arena, .d = d, .count = binding_count, .next_binding = binding_count};
    size_t capacity = 0;
    void *data = NULL;
    if (!uslc_reserve(&data, &capacity, binding_count ? binding_count : 1, sizeof *e.environment,
                      d))
        return false;
    e.environment = data;
    for (size_t i = 0; i < binding_count; ++i)
        e.environment[i] = NULL;
    for (const UslcAst *a = program->left; a; a = a->next) {
        if (a->kind != USLC_AST_DECLARATION || a->binding >= binding_count) {
            uslc_diagnose(d, 70, "E4090", "internal", a->span, "invalid declaration binding");
            break;
        }
        const UslcValue *closed = close_ast(&e, a->left, 0);
        const UslcValue *v = closed ? evaluate(&e, closed, 0) : NULL;
        if (!v)
            break;
        e.environment[a->binding] = v;
    }
    if (!d->exit_code) {
        const UslcValue *closed = close_ast(&e, program->right->left, 0);
        *result = closed ? evaluate(&e, closed, 0) : NULL;
    }
    free(e.environment);
    return !d->exit_code;
}
