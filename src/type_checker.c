#include "uslc/type_checker.h"
typedef struct {
    const UslcSource *source;
    UslcSymbolTable *table;
    UslcDiagnostics *diagnostics;
    size_t next_binding;
} Checker;
static bool binding(Checker *c, UslcAst *a) {
    if (c->next_binding == SIZE_MAX)
        return uslc_oom(c->diagnostics);
    a->binding = c->next_binding++;
    return true;
}
static bool check(Checker *c, UslcAst *a, size_t depth) {
    UslcDiagnostics *d = c->diagnostics;
    if (!uslc_depth(d, depth, a->span))
        return false;
    if (a->kind == USLC_AST_INTEGER) {
        a->type = USLC_TYPE_INTEGER;
        return true;
    }
    if (a->kind == USLC_AST_NAME) {
        UslcSymbol symbol;
        if (!uslc_symbols_find(c->table, c->source, a->token, &symbol))
            return uslc_diagnose(d, 3, "T3001", "typing", a->token.span, "undefined identifier");
        a->type = symbol.type;
        a->binding = symbol.binding;
        return true;
    }
    if (a->kind == USLC_AST_SET) {
        size_t saved = c->table->count;
        if (!binding(c, a) ||
            !uslc_symbols_push(c->table, (UslcSymbol){a->token, USLC_TYPE_INTEGER, a->binding}, d))
            return false;
        bool ok = check(c, a->left, depth + 1);
        c->table->count = saved;
        if (!ok)
            return false;
        if (a->left->type != USLC_TYPE_BOOLEAN)
            return uslc_diagnose(d, 3, "T3003", "typing", a->span, "set predicate must be boolean");
        a->type = USLC_TYPE_SET;
        return true;
    }
    if (!check(c, a->left, depth + 1))
        return false;
    if (a->right && !check(c, a->right, depth + 1))
        return false;
    UslcType left = USLC_TYPE_INTEGER, right = USLC_TYPE_INTEGER, result = USLC_TYPE_INTEGER;
    switch (a->kind) {
    case USLC_AST_ARITHMETIC:
        break;
    case USLC_AST_SET_BINARY:
        left = right = result = USLC_TYPE_SET;
        break;
    case USLC_AST_COMPARISON:
        result = USLC_TYPE_BOOLEAN;
        break;
    case USLC_AST_MEMBERSHIP:
        right = USLC_TYPE_SET;
        result = USLC_TYPE_BOOLEAN;
        break;
    case USLC_AST_LOGICAL:
    case USLC_AST_NOT:
        left = right = result = USLC_TYPE_BOOLEAN;
        break;
    default:
        return uslc_diagnose(d, 70, "I9003", "internal", a->span,
                             "invalid expression node in checker");
    }
    if (a->left->type != left || (a->right && a->right->type != right)) {
        a->type = USLC_TYPE_ERROR;
        return uslc_diagnose(
            d, 3, "T3003", "typing", a->token.span, "operator '%s' expects %s%s%s; found %s%s%s",
            uslc_token_spelling(a->token.kind), uslc_type_name(left), a->right ? " and " : "",
            a->right ? uslc_type_name(right) : "", uslc_type_name(a->left->type),
            a->right ? " and " : "", a->right ? uslc_type_name(a->right->type) : "");
    }
    a->type = result;
    return true;
}
bool uslc_type_check(UslcAst *program, const UslcSource *source, UslcSymbolTable *globals,
                     size_t *binding_count, UslcDiagnostics *d) {
    Checker c = {source, globals, d, 0};
    for (UslcAst *a = program->left; a; a = a->next) {
        UslcSymbol previous;
        if (uslc_symbols_find(globals, source, a->token, &previous))
            return uslc_diagnose(d, 3, "T3002", "typing", a->token.span,
                                 "duplicate declaration (previous declaration at %zu:%zu)",
                                 previous.name.span.start.line, previous.name.span.start.column);
        if (!check(&c, a->left, 0))
            return false;
        if (a->declared_type != a->left->type)
            return uslc_diagnose(d, 3, "T3004", "typing", a->left->span,
                                 "initializer has type %s; declaration requires %s",
                                 uslc_type_name(a->left->type), uslc_type_name(a->declared_type));
        if (!binding(&c, a) ||
            !uslc_symbols_push(globals, (UslcSymbol){a->token, a->declared_type, a->binding}, d))
            return false;
    }
    if (!check(&c, program->right->left, 0))
        return false;
    *binding_count = c.next_binding;
    return true;
}
