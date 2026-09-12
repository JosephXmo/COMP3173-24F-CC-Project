#include "test.h"
USLC_TEST(core_first_follow) {
    UslcGrammar g;
    UslcDiagnostics d = {0};
    uslc_grammar_core(&g);
    USLC_EXPECT(uslc_grammar_analyze(&g, &d));
    USLC_EXPECT(g.count == 34);
    USLC_EXPECT(g.first[USLC_S] == ((UINT64_C(1) << USLC_LET) | (UINT64_C(1) << USLC_SHOW)));
    USLC_EXPECT(g.follow[USLC_S] == (UINT64_C(1) << USLC_EOF));
    for (size_t i = 0; i < g.symbols; ++i)
        USLC_EXPECT(!g.nullable[i]);
}
USLC_TEST(nullable_recursive_grammar) {
    UslcGrammar g = {
        .terminals = 3,
        .symbols = 7,
        .count = 6,
        .eof = 2,
        .names = {"a", "b", "$", "S'", "S", "A", "B"},
        .productions = {
            {3, 1, {4}}, {4, 2, {5, 6}}, {5, 2, {0, 5}}, {5, 0, {0}}, {6, 2, {1, 6}}, {6, 0, {0}}}};
    UslcDiagnostics d = {0};
    USLC_EXPECT(uslc_grammar_analyze(&g, &d));
    USLC_EXPECT(g.nullable[3] && g.nullable[4] && g.nullable[5] && g.nullable[6]);
    USLC_EXPECT(g.first[4] == 3);
    USLC_EXPECT(g.follow[5] == 6);
    USLC_EXPECT(g.follow[6] == 4);
    UslcSlr slr = {0};
    USLC_EXPECT(uslc_slr_build(&slr, &g, &d));
    uslc_slr_free(&slr);
}
USLC_TEST(invalid_grammar) {
    UslcGrammar g;
    UslcDiagnostics d = {0};
    uslc_grammar_core(&g);
    g.productions[8].rhs[0] = 64;
    USLC_EXPECT(!uslc_grammar_analyze(&g, &d));
    USLC_EXPECT_STREQ("I9002", d.code);
}
void uslc_test_grammar(void) {
    USLC_RUN(core_first_follow);
    USLC_RUN(nullable_recursive_grammar);
    USLC_RUN(invalid_grammar);
}
