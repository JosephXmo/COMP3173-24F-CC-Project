#include "uslc/evaluator.h"
#include <inttypes.h>
#include <stdio.h>
#include "parser.h"
#include "uslc/parse_context.h"
static_assert(alignof(UslcValue)<=alignof(max_align_t));
bool uslc_checked_add(int64_t a,int64_t b,int64_t *r) {
    if ((b>0 && a>INT64_MAX-b) || (b<0 && a<INT64_MIN-b)) return false;
    *r=a+b;return true;
}
bool uslc_checked_sub(int64_t a,int64_t b,int64_t *r) {
    if ((b<0 && a>INT64_MAX+b) || (b>0 && a<INT64_MIN+b)) return false;
    *r=a-b;return true;
}
bool uslc_checked_mul(int64_t a,int64_t b,int64_t *r) {
    if (a>0 ? (b>0 ? a>INT64_MAX/b : b<INT64_MIN/a) :
        (a<0 && (b>0 ? a<INT64_MIN/b : b<0 && a<INT64_MAX/b))) return false;
    *r=a*b;return true;
}
static bool uslc_eval_guard(UslcParseContext *c,UslcSpan span,size_t depth) {
    if (depth>USLC_DEPTH_LIMIT) uslc_diagnose(&c->diagnostics,USLC_INTERNAL,"I9004",span,"symbolic tree-depth limit exceeded (255)");
    return !c->diagnostics.status;
}
static const UslcValue *uslc_value_new(UslcParseContext *c,UslcValue v) {
    v.height=1;
    if (v.left) v.height=v.left->height+1;
    if (v.right && v.right->height>=v.height) v.height=v.right->height+1;
    if (!uslc_eval_guard(c,v.span,v.height)) return NULL;
    UslcValue *p=uslc_arena_alloc(&c->value_arena,sizeof *p);
    if (p) *p=v;
    return p;
}
static const UslcValue *uslc_materialize(UslcParseContext *c,UslcAst *a,size_t depth) {
    if (!uslc_eval_guard(c,a->span,depth)) return NULL;
    UslcValue v={.type=a->type,.span=a->token ? a->token->span : a->span,.op=a->op,.binding=a->binding};
    if (a->kind==USLC_INT_LITERAL) { v.kind=USLC_VALUE_INT;v.integer=a->integer; }
    else if (a->kind==USLC_NAME) {
        for (size_t i=0;i<c->symbols.count;++i) if (c->symbols.items[i].binding==a->binding) {
            if (!c->symbols.items[i].value) uslc_diagnose(&c->diagnostics,USLC_INTERNAL,"E4090",a->span,"uninitialized runtime binding");
            return c->symbols.items[i].value;
        }
        v.kind=USLC_VALUE_VAR;
    } else {
        v.kind=a->kind==USLC_SET_LITERAL ? USLC_VALUE_SET : a->kind==USLC_NOT ? USLC_VALUE_NOT : USLC_VALUE_BINARY;
        v.left=uslc_materialize(c,a->left,depth+1);
        if (!v.left) return NULL;
        if (a->right) { v.right=uslc_materialize(c,a->right,depth+1); if (!v.right) return NULL; }
    }
    return uslc_value_new(c,v);
}
/* Substitution uses globally unique binding IDs; an inner binder never
   captures a value substituted for an outer representative. */
static const UslcValue *uslc_substitute(UslcParseContext *c,const UslcValue *v,size_t binding,const UslcValue *replacement,size_t depth) {
    if (!v || !uslc_eval_guard(c,v->span,depth)) return NULL;
    if (v->kind==USLC_VALUE_VAR && v->binding==binding) return replacement;
    UslcValue copy=*v;
    if (v->left) { copy.left=uslc_substitute(c,v->left,binding,replacement,depth+1); if (!copy.left) return NULL; }
    if (v->right) { copy.right=uslc_substitute(c,v->right,binding,replacement,depth+1); if (!copy.right) return NULL; }
    if (copy.left==v->left && copy.right==v->right) return v;
    return uslc_value_new(c,copy);
}
/* Fold safe closed arithmetic without evaluating a symbolic predicate. An
   overflowing expression remains deferred until membership reaches it, so
   a short-circuited branch cannot fail merely because a set is displayed. */
static const UslcValue *uslc_fold_constants(UslcParseContext *c,const UslcValue *v,size_t depth) {
    if (!v || !uslc_eval_guard(c,v->span,depth)) return NULL;
    UslcValue copy=*v;
    if (v->left) { copy.left=uslc_fold_constants(c,v->left,depth+1);if (!copy.left) return NULL; }
    if (v->right) { copy.right=uslc_fold_constants(c,v->right,depth+1);if (!copy.right) return NULL; }
    if (copy.kind==USLC_VALUE_BINARY && copy.left->kind==USLC_VALUE_INT && copy.right->kind==USLC_VALUE_INT) {
        int64_t number=0;bool folded=false;
        if (v->op==PLUS) folded=uslc_checked_add(copy.left->integer,copy.right->integer,&number);
        else if (v->op==MINUS) folded=uslc_checked_sub(copy.left->integer,copy.right->integer,&number);
        else if (v->op==STAR) folded=uslc_checked_mul(copy.left->integer,copy.right->integer,&number);
        if (folded) return uslc_value_new(c,(UslcValue){.kind=USLC_VALUE_INT,.type=USLC_INTEGER,.integer=number,.span=v->span});
    }
    if (copy.left==v->left && copy.right==v->right) return v;
    return uslc_value_new(c,copy);
}
static const UslcValue *uslc_resolve(UslcParseContext *c,const UslcValue *v,size_t depth) {
    if (!v || !uslc_eval_guard(c,v->span,depth)) return NULL;
    if (v->kind==USLC_VALUE_INT || v->kind==USLC_VALUE_BOOL) return v;
    if (v->kind==USLC_VALUE_SET) return uslc_fold_constants(c,v,depth);
    if (v->kind==USLC_VALUE_VAR) { uslc_diagnose(&c->diagnostics,USLC_INTERNAL,"E4002",v->span,"unbound set representative"); return NULL; }
    const UslcValue *l=uslc_resolve(c,v->left,depth+1);
    if (!l) return NULL;
    UslcValue out={.span=v->span,.type=v->type};
    if (v->kind==USLC_VALUE_NOT) { out.kind=USLC_VALUE_BOOL;out.boolean=!l->boolean;return uslc_value_new(c,out); }
    if ((v->op==AND && !l->boolean) || (v->op==OR && l->boolean)) return l;
    const UslcValue *r=uslc_resolve(c,v->right,depth+1);
    if (!r) return NULL;
    if (v->op==MEMBER) {
        const UslcValue *predicate=uslc_substitute(c,r->left,r->binding,l,depth+1);
        return uslc_resolve(c,predicate,depth+1);
    }
    if (v->op==UNION || v->op==INTERSECTION) {
        size_t binding=c->next_binding++;
        const UslcValue *rep=uslc_value_new(c,(UslcValue){.kind=USLC_VALUE_VAR,.type=USLC_INTEGER,.binding=binding,.span=v->span});
        if (!rep) return NULL;
        const UslcValue *a=uslc_substitute(c,l->left,l->binding,rep,depth+1);
        const UslcValue *b=uslc_substitute(c,r->left,r->binding,rep,depth+1);
        if (!a || !b) return NULL;
        const UslcValue *predicate=uslc_value_new(c,(UslcValue){.kind=USLC_VALUE_BINARY,.type=USLC_BOOLEAN,
            .op=v->op==UNION ? OR : AND,.left=a,.right=b,.span=v->span});
        if (!predicate) return NULL;
        out.kind=USLC_VALUE_SET;out.binding=binding;out.left=predicate;
        return uslc_value_new(c,out);
    }
    out.kind=USLC_VALUE_BOOL;
    switch(v->op) {
    case AND:case OR:out.boolean=r->boolean;break;
    case LT:out.boolean=l->integer<r->integer;break;
    case GT:out.boolean=l->integer>r->integer;break;
    case EQ:out.boolean=l->integer==r->integer;break;
    case PLUS:case MINUS:case STAR: {
        out.kind=USLC_VALUE_INT;
        bool ok=v->op==PLUS ? uslc_checked_add(l->integer,r->integer,&out.integer) :
            v->op==MINUS ? uslc_checked_sub(l->integer,r->integer,&out.integer) : uslc_checked_mul(l->integer,r->integer,&out.integer);
        if (!ok) {
            char message[128];snprintf(message,sizeof message,"integer overflow in '%s'",uslc_operator_name(v->op));
            uslc_diagnose(&c->diagnostics,USLC_EVALUATION,"E4001",v->span,message);return NULL;
        }
        break;
    }
    default:uslc_diagnose(&c->diagnostics,USLC_INTERNAL,"E4090",v->span,"invalid evaluator operation");return NULL;
    }
    return uslc_value_new(c,out);
}
bool uslc_evaluate(UslcParseContext *c) {
    if (!c->checked) return false;
    for (UslcAst *d=c->ast->left;d;d=d->next) {
        const UslcValue *term=uslc_materialize(c,d->left,1);
        d->value=uslc_resolve(c,term,1);
        if (!d->value) return false;
        for (size_t i=0;i<c->symbols.count;++i) if (c->symbols.items[i].binding==d->binding) c->symbols.items[i].value=d->value;
    }
    UslcAst *show=c->ast->right;
    const UslcValue *term=uslc_materialize(c,show->left,1);
    c->result=uslc_resolve(c,term,1);show->value=c->result;
    c->evaluated=c->result && !c->diagnostics.status;return c->evaluated;
}
