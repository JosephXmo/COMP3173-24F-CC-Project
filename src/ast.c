#include "uslc/ast.h"
const char *uslc_type_name(UslcType type) {
    static const char *const names[] = {"void", "integer", "boolean", "set", "error"};
    return names[type];
}
const char *uslc_ast_kind_name(UslcAstKind kind) {
    static const char *const names[] = {"Program",          "Declaration",   "ShowCommand",
                                        "IntegerLiteral",   "Name",          "SetLiteral",
                                        "BinaryArithmetic", "BinarySet",     "Comparison",
                                        "Membership",       "LogicalBinary", "LogicalNot"};
    return names[kind];
}
static UslcAst *new_node(UslcArena *arena, UslcAstKind kind, const UslcParseNode *p,
                         UslcDiagnostics *d) {
    UslcAst *a = uslc_arena_alloc(arena, sizeof *a, d);
    if (a) {
        a->kind = kind;
        a->span = p->span;
        a->binding = SIZE_MAX;
    }
    return a;
}
static UslcAst *lower(const UslcParseNode *p, UslcArena *arena, UslcDiagnostics *d, size_t depth) {
    if (!p || !uslc_depth(d, depth, p->span))
        return NULL;
    UslcAstKind kind;
    switch (p->production) {
    case 8:
    case 12:
    case 21:
    case 23:
    case 24:
    case 32:
    case 33:
        return lower(p->children[0], arena, d, depth + 1);
    case 17:
    case 25: {
        UslcAst *a = lower(p->children[1], arena, d, depth + 1);
        if (a)
            a->span = p->span;
        return a;
    }
    case 1:
    case 2:
        kind = USLC_AST_PROGRAM;
        break;
    case 5:
        kind = USLC_AST_DECLARATION;
        break;
    case 15:
        kind = USLC_AST_INTEGER;
        break;
    case 16:
        kind = USLC_AST_NAME;
        break;
    case 18:
        kind = USLC_AST_SET;
        break;
    case 9:
    case 13:
        kind = USLC_AST_SET_BINARY;
        break;
    case 10:
    case 11:
    case 14:
        kind = USLC_AST_ARITHMETIC;
        break;
    case 20:
    case 22:
        kind = USLC_AST_LOGICAL;
        break;
    case 26:
        kind = USLC_AST_NOT;
        break;
    case 27:
    case 28:
    case 29:
        kind = USLC_AST_COMPARISON;
        break;
    case 30:
        kind = USLC_AST_MEMBERSHIP;
        break;
    case 31:
        kind = USLC_AST_SHOW;
        break;
    default:
        uslc_diagnose(d, 70, "I9003", "internal", p->span, "unexpected production in AST lowering");
        return NULL;
    }
    UslcAst *a = new_node(arena, kind, p, d);
    if (!a)
        return NULL;
    if (kind == USLC_AST_PROGRAM) {
        if (p->production == 1) {
            const UslcParseNode *list = p->children[0];
            UslcAst **tail = &a->left;
            for (;;) {
                if (list->production != 3 && list->production != 4) {
                    uslc_diagnose(d, 70, "I9003", "internal", list->span,
                                  "invalid declaration list");
                    return NULL;
                }
                *tail = lower(list->children[0], arena, d, depth + 1);
                if (!*tail)
                    return NULL;
                tail = &(*tail)->next;
                if (list->production == 4)
                    break;
                list = list->children[1];
            }
        }
        a->right = lower(p->children[p->production == 1 ? 1 : 0], arena, d, depth + 1);
    } else if (kind == USLC_AST_DECLARATION) {
        a->token = p->children[2]->token;
        a->declared_type = p->children[1]->production == 6 ? USLC_TYPE_INTEGER : USLC_TYPE_SET;
        a->left = lower(p->children[4], arena, d, depth + 1);
    } else if (kind == USLC_AST_INTEGER || kind == USLC_AST_NAME)
        a->token = p->children[0]->token;
    else if (kind == USLC_AST_SET) {
        a->token = p->children[1]->children[0]->token;
        a->left = lower(p->children[2], arena, d, depth + 1);
    } else if (kind == USLC_AST_SHOW || kind == USLC_AST_NOT) {
        a->token = p->children[0]->token;
        a->left = lower(p->children[1], arena, d, depth + 1);
    } else {
        a->token = p->children[1]->token;
        a->left = lower(p->children[0], arena, d, depth + 1);
        if (!d->exit_code)
            a->right = lower(p->children[2], arena, d, depth + 1);
    }
    return d->exit_code ? NULL : a;
}
static bool validate(const UslcParseNode *p, const UslcGrammar *g, UslcDiagnostics *d,
                     size_t depth) {
    if (!p)
        return uslc_diagnose(d, 70, "I9003", "internal", (UslcSpan){0}, "missing parse tree child");
    if (!uslc_depth(d, depth, p->span))
        return false;
    if (p->production == SIZE_MAX) {
        if (p->symbol < g->terminals && p->symbol == (size_t)p->token.kind && !p->count)
            return true;
    } else if (p->production < g->count) {
        const UslcProduction *rule = &g->productions[p->production];
        if (p->symbol == rule->lhs && p->count == rule->length) {
            for (size_t i = 0; i < p->count; ++i) {
                if (!validate(p->children[i], g, d, depth + 1))
                    return false;
                if (p->children[i]->symbol != rule->rhs[i])
                    return uslc_diagnose(d, 70, "I9003", "internal", p->span,
                                         "parse tree child has wrong symbol");
            }
            return true;
        }
    }
    return uslc_diagnose(d, 70, "I9003", "internal", p->span,
                         "parse tree does not match its production");
}
bool uslc_ast_lower(const UslcParseNode *root, UslcArena *arena, UslcAst **ast,
                    UslcDiagnostics *d) {
    UslcGrammar grammar;
    uslc_grammar_core(&grammar);
    *ast = NULL;
    if (!validate(root, &grammar, d, 0))
        return false;
    if (root->symbol != USLC_S)
        return uslc_diagnose(d, 70, "I9003", "internal", root->span, "expected program root");
    *ast = lower(root, arena, d, 0);
    return *ast != NULL;
}
