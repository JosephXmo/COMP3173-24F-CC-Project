#include "test.h"
#include "parser.h"
static void uslc_collect_productions(const UslcParseNode *n,bool *seen) {
    if (n->production_id>0) seen[n->production_id]=true;
    for (size_t i=0;i<n->child_count;++i) {
        USLC_EXPECT(n->children[i]!=NULL);
        if (i) USLC_EXPECT(n->children[i-1]->span.end.offset<=n->children[i]->span.start.offset);
        uslc_collect_productions(n->children[i],seen);
    }
}
USLC_TEST(uslc_test_parser) {
    UslcParseContext c;bool seen[34]={0};
    const char *valid[]={"show 3.","let int a be (3). let set b be {x:x>1}. show (a+2*3-1<9)&!(a)>8|a=2|a@b.",
        "show {x:x<3} U {y:y>4} I {z:z=8}.","show !1=2.","show (1+2)*3."};
    for (size_t i=0;i<sizeof valid/sizeof *valid;++i) {
        bool ok=uslc_test_compile(&c,valid[i],2);USLC_EXPECT(ok);
        if (ok) { uslc_collect_productions(c.root,seen);USLC_EXPECT(c.ast!=NULL); }
        uslc_context_free(&c);
    }
    for (size_t i=1;i<34;++i) USLC_EXPECT(seen[i]);
    USLC_EXPECT(uslc_test_compile(&c,"show 9-3-2*2.",2));
    if (c.ast) {
        UslcAst *a=c.ast->right->left;
        USLC_EXPECT_EQ_INT(MINUS,a->op);USLC_EXPECT_EQ_INT(MINUS,a->left->op);USLC_EXPECT_EQ_INT(STAR,a->right->op);
        USLC_EXPECT_EQ_INT(2,c.root->production_id);USLC_EXPECT_STREQ("DOT",c.root->children[1]->symbol);
    }
    uslc_context_free(&c);
    const char *invalid[]={"","show","show 1","show 1..","show 1. show 2.","let int a be 1.","show -1.","show 1<2<3.","show !!1=2.","show !(1=2).","show {x:1}.","let int a be 1<2. show a.","show (1+2.","show 1.let int a be 1."};
    for (size_t i=0;i<sizeof invalid/sizeof *invalid;++i) {
        USLC_EXPECT(!uslc_test_compile(&c,invalid[i],2));USLC_EXPECT_EQ_INT(2,c.diagnostics.status);USLC_EXPECT(c.diagnostics.note[0]);uslc_context_free(&c);
    }
    USLC_EXPECT(!uslc_test_compile(&c,"show\n.",2));USLC_EXPECT_STREQ("P2001",c.diagnostics.code);USLC_EXPECT_EQ_INT(2,c.diagnostics.span.start.line);uslc_context_free(&c);
    USLC_EXPECT(!uslc_test_compile(&c,"show 1",2));USLC_EXPECT_STREQ("P2002",c.diagnostics.code);USLC_EXPECT_EQ_INT(6,c.diagnostics.span.start.offset);uslc_context_free(&c);
    char deep[6000];size_t n=0;memcpy(deep,"show ",5);n=5;
    for (int i=0;i<300;++i) deep[n++]='(';
    deep[n++]='1';for (int i=0;i<300;++i) deep[n++]=')';deep[n++]='.';deep[n]='\0';
    USLC_EXPECT(!uslc_test_compile(&c,deep,2));USLC_EXPECT_STREQ("I9004",c.diagnostics.code);uslc_context_free(&c);
    n=5;for (int i=0;i<300;++i) { deep[n++]='1';deep[n++]='+'; }deep[n++]='1';deep[n++]='.';deep[n]='\0';
    USLC_EXPECT(!uslc_test_compile(&c,deep,2));USLC_EXPECT_STREQ("I9004",c.diagnostics.code);uslc_context_free(&c);
    uslc_context_init(&c);USLC_EXPECT(!uslc_lower(&c));USLC_EXPECT_STREQ("I9003",c.diagnostics.code);uslc_context_free(&c);
}
