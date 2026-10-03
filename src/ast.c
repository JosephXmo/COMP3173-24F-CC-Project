#include "uslc/ast.h"
#include "uslc/parse_context.h"
static_assert(alignof(UslcAst)<=alignof(max_align_t));
const char *uslc_type_name(UslcType t) {
    switch(t) { case USLC_INTEGER:return "integer"; case USLC_BOOLEAN:return "boolean";
    case USLC_SET:return "set"; case USLC_VOID:return "void"; default:return "error"; }
}
const char *uslc_ast_kind_name(UslcAstKind k) {
    static const char *const names[]={"Program","Declaration","ShowCommand","IntegerLiteral",
        "Name","SetLiteral","BinaryArithmetic","BinarySet","Comparison","Membership","LogicalBinary","LogicalNot"};
    return (size_t)k<sizeof names/sizeof *names ? names[k] : "Invalid";
}
static UslcAst *uslc_ast_new(UslcParseContext *c, UslcAstKind kind, const UslcParseNode *p) {
    UslcAst *a=uslc_arena_alloc(&c->ast_arena,sizeof *a);
    if (a) { a->kind=kind; a->span=p->span; a->height=1; }
    return a;
}
static UslcAst *uslc_lower_node(UslcParseContext *c, const UslcParseNode *p, size_t depth) {
    static const size_t arities[]={0,3,2,2,1,6,1,1,1,3,3,3,1,3,3,1,1,3,4,2,3,1,3,1,1,3,2,3,3,3,3,2,1,1};
    if (c->diagnostics.status) return NULL;
    if (depth>USLC_DEPTH_LIMIT) { uslc_diagnose(&c->diagnostics,USLC_INTERNAL,"I9004",p ? p->span : uslc_span_origin(),"AST depth limit exceeded"); return NULL; }
    if (!p || p->production_id<1 || p->production_id>33 || p->child_count!=arities[p->production_id]) goto malformed;
    for (size_t i=0;i<p->child_count;++i) if (!p->children[i]) goto malformed;
    int id=p->production_id;
    if (id==8 || id==12 || id==21 || id==23 || id==24 || id==32 || id==33 || id==4)
        return uslc_lower_node(c,p->children[0],depth+1);
    if (id==17 || id==25) {
        UslcAst *a=uslc_lower_node(c,p->children[1],depth+1);
        if (a) a->span=p->span;
        return a;
    }
    if (id==3) {
        UslcAst *a=uslc_lower_node(c,p->children[0],depth+1);
        if (a) a->next=uslc_lower_node(c,p->children[1],depth+1);
        return a;
    }
    UslcAstKind kind;
    switch(id) {
    case 1:case 2:kind=USLC_PROGRAM;break;
    case 5:kind=USLC_DECLARATION;break;
    case 9:case 13:kind=USLC_SET_BINARY;break;
    case 10:case 11:case 14:kind=USLC_ARITHMETIC;break;
    case 15:kind=USLC_INT_LITERAL;break;
    case 16:kind=USLC_NAME;break;
    case 18:kind=USLC_SET_LITERAL;break;
    case 20:case 22:kind=USLC_LOGICAL;break;
    case 26:kind=USLC_NOT;break;
    case 27:case 28:case 29:kind=USLC_COMPARISON;break;
    case 30:kind=USLC_MEMBERSHIP;break;
    case 31:kind=USLC_SHOW;break;
    default:goto malformed;
    }
    UslcAst *a=uslc_ast_new(c,kind,p);
    if (!a) return NULL;
    if (id==1 || id==2) {
        if (id==1) a->left=uslc_lower_node(c,p->children[0],depth+1);
        a->right=uslc_lower_node(c,p->children[id==1 ? 1 : 0],depth+1);
    } else if (id==5) {
        a->token=p->children[2]; a->declared_type=p->children[1]->production_id==6 ? USLC_INTEGER : USLC_SET;
        a->left=uslc_lower_node(c,p->children[4],depth+1);
    } else if (id==15 || id==16) {
        a->token=p->children[0]; a->integer=a->token->integer;
    } else if (id==18) {
        if (p->children[1]->child_count!=2 || !p->children[1]->children[0]) goto malformed;
        a->token=p->children[1]->children[0]; a->left=uslc_lower_node(c,p->children[2],depth+1);
    } else if (id==26 || id==31) {
        a->token=p->children[0]; a->op=a->token->token_kind;
        a->left=uslc_lower_node(c,p->children[1],depth+1);
    } else {
        a->token=p->children[1]; a->op=a->token->token_kind;
        a->left=uslc_lower_node(c,p->children[0],depth+1);
        a->right=uslc_lower_node(c,p->children[2],depth+1);
    }
    if (a->left) a->height=a->left->height+1;
    if (a->right && a->right->height>=a->height) a->height=a->right->height+1;
    return c->diagnostics.status ? NULL : a;
malformed:
    uslc_diagnose(&c->diagnostics,USLC_INTERNAL,"I9003",p ? p->span : uslc_span_origin(),"malformed parse tree"); return NULL;
}
bool uslc_lower(UslcParseContext *c) {
    c->ast=uslc_lower_node(c,c->root,1);
    c->lowered=c->ast && !c->diagnostics.status; return c->lowered;
}
