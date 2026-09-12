#include "uslc/type_checker.h"
typedef struct UslcScope { const UslcParseNode *name; size_t binding; const struct UslcScope *parent; } UslcScope;
static UslcType uslc_check_node(UslcParseContext *c, UslcAst *a, const UslcScope *scope, size_t depth) {
    if (c->diagnostics.status) return USLC_ERROR;
    if (!a || depth>USLC_DEPTH_LIMIT) {
        uslc_diagnose(&c->diagnostics,USLC_INTERNAL,"I9004",a ? a->span : uslc_span_origin(),"type-check depth limit exceeded"); return USLC_ERROR;
    }
    UslcType left=USLC_VOID,right=USLC_VOID,want_left=USLC_VOID,want_right=USLC_VOID;
    if (a->kind==USLC_INT_LITERAL) return a->type=USLC_INTEGER;
    if (a->kind==USLC_NAME) {
        for (const UslcScope *s=scope;s;s=s->parent) if (uslc_name_equal(c,s->name,a->token)) {
            a->binding=s->binding; return a->type=USLC_INTEGER;
        }
        UslcSymbol *symbol=uslc_symbol_find(c,a->token);
        if (!symbol) { uslc_diagnose(&c->diagnostics,USLC_TYPING,"T3001",a->span,"undefined identifier"); return a->type=USLC_ERROR; }
        a->binding=symbol->binding; return a->type=symbol->type;
    }
    if (a->kind==USLC_SET_LITERAL) {
        a->binding=c->next_binding++;
        UslcScope local={a->token,a->binding,scope};
        left=uslc_check_node(c,a->left,&local,depth+1);
        if (left==USLC_BOOLEAN) return a->type=USLC_SET;
        goto mismatch;
    }
    left=uslc_check_node(c,a->left,scope,depth+1);
    if (a->right) right=uslc_check_node(c,a->right,scope,depth+1);
    if (c->diagnostics.status) return a->type=USLC_ERROR;
    switch(a->kind) {
    case USLC_ARITHMETIC:want_left=want_right=USLC_INTEGER;a->type=USLC_INTEGER;break;
    case USLC_SET_BINARY:want_left=want_right=USLC_SET;a->type=USLC_SET;break;
    case USLC_COMPARISON:want_left=want_right=USLC_INTEGER;a->type=USLC_BOOLEAN;break;
    case USLC_MEMBERSHIP:want_left=USLC_INTEGER;want_right=USLC_SET;a->type=USLC_BOOLEAN;break;
    case USLC_LOGICAL:want_left=want_right=USLC_BOOLEAN;a->type=USLC_BOOLEAN;break;
    case USLC_NOT:want_left=USLC_BOOLEAN;a->type=USLC_BOOLEAN;break;
    default:goto mismatch;
    }
    if (left==want_left && right==want_right) return a->type;
mismatch:
    uslc_diagnose(&c->diagnostics,USLC_TYPING,"T3003",a->token ? a->token->span : a->span,"incompatible operand types");
    return a->type=USLC_ERROR;
}
bool uslc_type_check(UslcParseContext *c) {
    if (!c->lowered) return false;
    for (UslcAst *d=c->ast->left;d && !c->diagnostics.status;d=d->next) {
        if (uslc_symbol_find(c,d->token)) { uslc_diagnose(&c->diagnostics,USLC_TYPING,"T3002",d->token->span,"duplicate declaration"); break; }
        UslcType type=uslc_check_node(c,d->left,NULL,1);
        if (c->diagnostics.status) break;
        if (type!=d->declared_type) { uslc_diagnose(&c->diagnostics,USLC_TYPING,"T3004",d->left->span,"initializer type does not match declaration"); break; }
        d->binding=c->next_binding++;
        if (!uslc_symbol_add(c,(UslcSymbol){d->token,type,d->binding,NULL})) break;
    }
    if (!c->diagnostics.status) {
        UslcAst *show=c->ast->right;
        UslcType type=uslc_check_node(c,show->left,NULL,1);
        if (!c->diagnostics.status && type!=USLC_INTEGER && type!=USLC_BOOLEAN && type!=USLC_SET)
            uslc_diagnose(&c->diagnostics,USLC_TYPING,"T3005",show->span,"invalid show value");
    }
    c->checked=!c->diagnostics.status; return c->checked;
}
