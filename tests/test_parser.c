#include "test.h"
USLC_TEST(precedence_and_child_order) {
    UslcTestCompilation c;
    uslc_test_init(&c, "show 9-3-2*2 .");
    USLC_EXPECT(uslc_test_compile(&c, 2));
    USLC_EXPECT(c.parse->production == 2);
    USLC_EXPECT(c.parse->children[0]->symbol == USLC_C);
    USLC_EXPECT(c.parse->children[1]->token.kind == USLC_DOT);
    UslcAst *e = c.ast->right->left;
    USLC_EXPECT(e->token.kind == USLC_MINUS);
    USLC_EXPECT(e->left->token.kind == USLC_MINUS);
    USLC_EXPECT(e->right->token.kind == USLC_STAR);
    uslc_test_free(&c);
}
USLC_TEST(parser_accepts) {
    const char *inputs[] = {"show 1 .", "let int a be 1.let int b be 2.show a+b.",
                            "show ((1+2)) > 2 & !(3=4) .", "show {x: (x>1|x<0)&x<8}I{y:y>0} .",
                            "show {x:x@{y:y>x}} ."};
    /* NOT applies to R, so parenthesized predicates after NOT are intentionally excluded. */
    inputs[2] = "show ((1+2)) > 2 & !3=4 .";
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        UslcTestCompilation c;
        uslc_test_init(&c, inputs[i]);
        bool ok = uslc_test_compile(&c, 2);
        uslc_test_free(&c);
        USLC_EXPECT(ok);
    }
}
USLC_TEST(parser_rejects) {
    const char *inputs[] = {"",           "show 1",        "show 1..",    "show 1.show 2.",
                            "show -1.",   "show ! (1<2).", "show !!1<2.", "let int x be 1.",
                            "show 1<2<3."};
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        UslcTestCompilation c;
        uslc_test_init(&c, inputs[i]);
        bool ok = uslc_test_compile(&c, 2);
        int status = c.d.exit_code;
        bool expected = strstr(c.d.message, "expected:") != NULL;
        uslc_test_free(&c);
        USLC_EXPECT(!ok);
        USLC_EXPECT(status == 2);
        USLC_EXPECT(expected);
    }
}
USLC_TEST(parser_allocation_failure) {
    UslcTestCompilation c;
    uslc_test_init(&c, "show 1.");
    c.parse_arena.fail_after = 0;
    USLC_EXPECT(!uslc_test_compile(&c, 2));
    USLC_EXPECT_STREQ("I9001", c.d.code);
    uslc_test_free(&c);
}
USLC_TEST(malformed_tree_and_ast_oom) {
    UslcTestCompilation c;
    uslc_test_init(&c, "show 1.");
    c.ast_arena.fail_after = 0;
    USLC_EXPECT(!uslc_test_compile(&c, 2));
    USLC_EXPECT_STREQ("I9001", c.d.code);
    uslc_diagnostics_init(&c.d);
    c.ast_arena.fail_after = SIZE_MAX;
    c.parse->count = 0;
    USLC_EXPECT(!uslc_ast_lower(c.parse, &c.ast_arena, &c.ast, &c.d));
    USLC_EXPECT_STREQ("I9003", c.d.code);
    uslc_test_free(&c);
}
USLC_TEST(epsilon_reduction) {
    UslcGrammar g = {.terminals = 1,
                     .symbols = 3,
                     .count = 2,
                     .eof = 0,
                     .names = {"$", "S'", "S"},
                     .productions = {{1, 1, {2}}, {2, 0, {0}}}};
    UslcDiagnostics d = {0};
    UslcSlr slr = {0};
    UslcArena arena;
    uslc_arena_init(&arena);
    UslcToken eof = {.kind = 0, .span = {{1, 1, 0}, {1, 1, 0}}};
    UslcTokens tokens = {.data = &eof, .count = 1};
    UslcParseNode *root = NULL;
    size_t nodes = 0;
    USLC_EXPECT(uslc_grammar_analyze(&g, &d));
    USLC_EXPECT(uslc_slr_build(&slr, &g, &d));
    USLC_EXPECT(uslc_parse(&slr, &tokens, &arena, &root, &nodes, &d, NULL, NULL));
    USLC_EXPECT(nodes == 1 && root->count == 0 && root->production == 1 &&
                root->span.start.line == 1);
    uslc_arena_free(&arena);
    uslc_slr_free(&slr);
}
USLC_TEST(nesting_limit) {
    char source[1024] = "show ";
    size_t n = strlen(source);
    for (size_t i = 0; i < 300; ++i)
        source[n++] = '(';
    source[n++] = '1';
    for (size_t i = 0; i < 300; ++i)
        source[n++] = ')';
    source[n++] = '.';
    source[n] = '\0';
    UslcTestCompilation c;
    uslc_test_init(&c, source);
    USLC_EXPECT(!uslc_test_compile(&c, 2));
    USLC_EXPECT_STREQ("I9004", c.d.code);
    uslc_test_free(&c);
}
void uslc_test_parser(void) {
    USLC_RUN(precedence_and_child_order);
    USLC_RUN(parser_accepts);
    USLC_RUN(parser_rejects);
    USLC_RUN(parser_allocation_failure);
    USLC_RUN(malformed_tree_and_ast_oom);
    USLC_RUN(epsilon_reduction);
    USLC_RUN(nesting_limit);
}
