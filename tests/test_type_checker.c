#include "test.h"
USLC_TEST(uslc_test_type_checker) {
    struct { const char *source,*code; } cases[]={
        {"show missing.","T3001"},{"let int x be x. show x.","T3001"},
        {"let int x be y. let int y be 1. show x.","T3001"},
        {"let int x be 1. let int x be 2. show x.","T3002"},
        {"let set x be 4. show x.","T3004"},{"let int x be {y:y>0}. show x.","T3004"},
        {"show {x:x>0}+1.","T3003"},{"show 1-{x:x>0}.","T3003"},{"show 1*{x:x>0}.","T3003"},
        {"show 1 U 2.","T3003"},{"show {x:x>0} I 2.","T3003"},
        {"show {x:x>0}<1.","T3003"},{"show 1>{x:x>0}.","T3003"},{"show {x:x>0}={y:y>0}.","T3003"},
        {"show 1@2.","T3003"},{"show {x:x>0}@{y:y>0}.","T3003"},
        {"show {x:x>0} U {y:y>x}.","T3001"},{"show true.","T3001"},
        {"show 1=1 | missing=0.","T3001"},{"show {x:x @ x}.","T3003"}
    };
    UslcParseContext c;
    for (size_t i=0;i<sizeof cases/sizeof *cases;++i) {
        USLC_EXPECT(!uslc_test_compile(&c,cases[i].source,3));USLC_EXPECT_EQ_INT(3,c.diagnostics.status);USLC_EXPECT_STREQ(cases[i].code,c.diagnostics.code);USLC_EXPECT(!c.evaluated);uslc_context_free(&c);
    }
    const char *valid[]={"let int x be 9. show {x:x<3}.","let set x be {x:x>0}. show 3@x.",
        "show {x:x @ {y:y>x}}.","show {x:x @ {x:x>2}}.",
        "let int x be 1. let int y be x+1. show y.","show 1<2&2>1|!1=0."};
    for (size_t i=0;i<sizeof valid/sizeof *valid;++i) { USLC_EXPECT(uslc_test_compile(&c,valid[i],3));USLC_EXPECT(c.checked);uslc_context_free(&c); }
    USLC_EXPECT(uslc_test_compile(&c,"let int x be 9. show {x:x<3}.",3));
    if (c.ast) {
        UslcAst *set=c.ast->right->left;
        USLC_EXPECT(set->binding!=c.ast->left->binding);USLC_EXPECT_EQ_INT(set->binding,set->left->left->binding);
        USLC_EXPECT_EQ_INT(USLC_SET,set->type);USLC_EXPECT_EQ_INT(USLC_VOID,c.ast->type);
    }
    uslc_context_free(&c);
}
