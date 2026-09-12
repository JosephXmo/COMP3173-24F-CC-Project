#include "test.h"
USLC_TEST(all_tokens) {
    UslcTestCompilation c;
    uslc_test_init(&c, ". let abc be int set U+-I*9(){}:|&!<>=@show");
    USLC_EXPECT(uslc_test_compile(&c, 1));
    USLC_EXPECT(c.tokens.count == 26);
    for (size_t i = 0; i < 26; ++i)
        USLC_EXPECT((size_t)c.tokens.data[i].kind == i);
    uslc_test_free(&c);
}
USLC_TEST(longest_match_and_spans) {
    UslcTestCompilation c;
    uslc_test_init(&c, "letbe be123+a\r\n\tshow\r0\n\f\v.");
    USLC_EXPECT(uslc_test_compile(&c, 1));
    const UslcTokenKind kinds[] = {USLC_ID,   USLC_BE,  USLC_NUM, USLC_PLUS, USLC_ID,
                                   USLC_SHOW, USLC_NUM, USLC_DOT, USLC_EOF};
    USLC_EXPECT(c.tokens.count == sizeof kinds / sizeof *kinds);
    for (size_t i = 0; i < c.tokens.count; ++i) {
        USLC_EXPECT(c.tokens.data[i].kind == kinds[i]);
        if (i)
            USLC_EXPECT(c.tokens.data[i - 1].span.end.offset <= c.tokens.data[i].span.start.offset);
    }
    USLC_EXPECT(c.tokens.data[0].span.end.column == 6);
    USLC_EXPECT(c.tokens.data[5].span.start.line == 2);
    USLC_EXPECT(c.tokens.data[5].span.start.column == 2);
    USLC_EXPECT(c.tokens.data[6].span.start.line == 3);
    USLC_EXPECT(c.tokens.data[7].span.start.line == 4);
    USLC_EXPECT(c.tokens.data[7].span.start.column == 3);
    uslc_test_free(&c);
}
USLC_TEST(lexical_errors) {
    const char *inputs[] = {
        "012", "00",      "9223372036854775808", "999999999999999999999999999999", "A", "_",
        "/",   "\xc3\xa9"};
    const char *codes[] = {"L1002", "L1002", "L1003", "L1003", "L1001", "L1001", "L1001", "L1001"};
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        UslcTestCompilation c;
        uslc_test_init(&c, inputs[i]);
        bool ok = uslc_test_compile(&c, 1);
        int status = c.d.exit_code;
        const char *code = c.d.code;
        uslc_test_free(&c);
        USLC_EXPECT(!ok);
        USLC_EXPECT(status == 1);
        USLC_EXPECT_STREQ(codes[i], code);
    }
    UslcTestCompilation c;
    uslc_test_init(&c, "9223372036854775807 0 -1 true false");
    USLC_EXPECT(uslc_test_compile(&c, 1));
    USLC_EXPECT(c.tokens.data[0].number == INT64_MAX);
    USLC_EXPECT(c.tokens.data[2].kind == USLC_MINUS);
    USLC_EXPECT(c.tokens.data[4].kind == USLC_ID);
    uslc_test_free(&c);
}
USLC_TEST(embedded_nul) {
    UslcSource s = {"nul", "a\0b", 3};
    UslcLexer l;
    UslcDiagnostics d = {0};
    uslc_lexer_init(&l, &s, &d);
    USLC_EXPECT(uslc_lexer_next(&l).kind == USLC_ID);
    USLC_EXPECT(uslc_lexer_next(&l).kind == USLC_ERROR);
    USLC_EXPECT(d.span.start.offset == 1);
}
void uslc_test_lexer(void) {
    USLC_RUN(all_tokens);
    USLC_RUN(longest_match_and_spans);
    USLC_RUN(lexical_errors);
    USLC_RUN(embedded_nul);
}
