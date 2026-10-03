#include "test.h"
USLC_TEST(type_errors) {
    const char *inputs[] = {"show x.",
                            "let int x be x.show x.",
                            "let int x be 1.let int x be 2.show x.",
                            "let set x be 4.show x.",
                            "let int x be {y:y>0}.show x.",
                            "show {x:x>0}+1.",
                            "show 1+{x:x>0}.",
                            "show 1-{x:x>0}.",
                            "show 1*{x:x>0}.",
                            "show 1U{x:x>0}.",
                            "show {x:x>0}I1.",
                            "show {x:x>0}<1.",
                            "show 1>{x:x>0}.",
                            "show {x:x>0}={y:y>0}.",
                            "show 1@2.",
                            "show {x:x>0}@{y:y>0}.",
                            "let set s be {x:x>0}.show x.",
                            "let set s be {x:y>0}.show s.",
                            "show true."};
    const char *codes[] = {"T3001", "T3001", "T3002", "T3004", "T3004", "T3003", "T3003",
                           "T3003", "T3003", "T3003", "T3003", "T3003", "T3003", "T3003",
                           "T3003", "T3003", "T3001", "T3001", "T3001"};
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        UslcTestCompilation c;
        uslc_test_init(&c, inputs[i]);
        bool ok = uslc_test_compile(&c, 3);
        const char *code = c.d.code;
        int status = c.d.exit_code;
        uslc_test_free(&c);
        USLC_EXPECT(!ok);
        USLC_EXPECT(status == 3);
        USLC_EXPECT_STREQ(codes[i], code);
    }
}
USLC_TEST(scopes_and_bindings) {
    UslcTestCompilation c;
    uslc_test_init(&c, "let set x be {x:x>0}.show {x:x@{x:x>0}}.");
    USLC_EXPECT(uslc_test_compile(&c, 3));
    USLC_EXPECT(c.symbols.count == 1);
    USLC_EXPECT(c.symbols.entries[0].type == USLC_TYPE_SET);
    UslcAst *outer = c.ast->right->left, *inner = outer->left->right;
    USLC_EXPECT(outer->binding != inner->binding);
    USLC_EXPECT(outer->left->left->binding == outer->binding);
    USLC_EXPECT(inner->left->left->binding == inner->binding);
    uslc_test_free(&c);
}
void uslc_test_type_checker(void) {
    USLC_RUN(type_errors);
    USLC_RUN(scopes_and_bindings);
}
