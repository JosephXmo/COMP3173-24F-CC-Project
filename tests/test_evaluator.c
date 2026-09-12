#include "test.h"
USLC_TEST(integer_limits) {
    int64_t r;
    USLC_EXPECT(!uslc_checked_add(INT64_MAX, 1, &r));
    USLC_EXPECT(!uslc_checked_add(INT64_MIN, -1, &r));
    USLC_EXPECT(uslc_checked_add(INT64_MIN, INT64_MAX, &r) && r == -1);
    USLC_EXPECT(!uslc_checked_sub(INT64_MIN, 1, &r));
    USLC_EXPECT(!uslc_checked_sub(INT64_MAX, -1, &r));
    USLC_EXPECT(uslc_checked_sub(INT64_MIN, INT64_MIN, &r) && r == 0);
    USLC_EXPECT(!uslc_checked_mul(INT64_MIN, -1, &r));
    USLC_EXPECT(!uslc_checked_mul(-1, INT64_MIN, &r));
    USLC_EXPECT(!uslc_checked_mul(INT64_MAX, 2, &r));
    USLC_EXPECT(!uslc_checked_mul(INT64_MIN, 2, &r));
    USLC_EXPECT(!uslc_checked_mul(-4611686018427387904LL, -2, &r));
    USLC_EXPECT(uslc_checked_mul(INT64_MIN, 1, &r) && r == INT64_MIN);
    USLC_EXPECT(uslc_checked_mul(INT64_MAX, 0, &r) && r == 0);
    for (int64_t a = -30; a <= 30; ++a)
        for (int64_t b = -30; b <= 30; ++b)
            USLC_EXPECT(uslc_checked_mul(a, b, &r) && r == a * b);
}
USLC_TEST(evaluation_cases) {
    const char *inputs[] = {"show 9-3-2*2.",
                            "show 0-9223372036854775807-1.",
                            "show 1<2|9223372036854775807+1>0.",
                            "show 1>2&9223372036854775807+1>0.",
                            "show !1=2.",
                            "show 3@({x:x>3}U{y:y<1}).",
                            "show 4@({x:x>3}I{y:y<5}).",
                            "let int x be 3.let set s be {x:x>1}.show x@s.",
                            "show 3@{x:4@{y:y>x}}.",
                            "show 3@{x:2@{x:x>1}}.",
                            "show 3@({x:4@{y:y>x}}I{y:y<4}).",
                            "show 3@{x:4@({y:y>x}U{z:z<0})}.",
                            "let set s be {x:x>0}.let set t be s I {x:x<5}.show 2@t.",
                            "show 2@{x:x>0|9223372036854775807+1>0}.",
                            "show {a:a>3}U{b:b>5}."};
    const char *expected[] = {"2",
                              "-9223372036854775808",
                              "true",
                              "false",
                              "true",
                              "false",
                              "true",
                              "true",
                              "true",
                              "true",
                              "true",
                              "true",
                              "true",
                              "true",
                              "{ x : ((x > 3) | (x > 5)) }"};
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        UslcTestCompilation c;
        uslc_test_init(&c, inputs[i]);
        bool ok = uslc_test_compile(&c, 4);
        if (!ok)
            fprintf(stderr, "input: %s; %s\n", inputs[i], c.d.message);
        char *s = ok ? uslc_value_display(c.result, &c.d) : NULL;
        bool equal = s && !strcmp(s, expected[i]);
        if (!equal)
            fprintf(stderr, "input: %s; expected: %s; actual: %s\n", inputs[i], expected[i],
                    s ? s : "NULL");
        free(s);
        uslc_test_free(&c);
        USLC_EXPECT(ok);
        USLC_EXPECT(equal);
    }
}
USLC_TEST(evaluation_errors) {
    const char *inputs[] = {"show 9223372036854775807+1.", "show 0-9223372036854775807-2.",
                            "show (0-9223372036854775807-1)*(0-1).", "show 9223372036854775807*2.",
                            "show 1@{x:x>2|9223372036854775807+1>0}."};
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
        UslcTestCompilation c;
        uslc_test_init(&c, inputs[i]);
        bool ok = uslc_test_compile(&c, 4);
        int status = c.d.exit_code;
        const char *code = c.d.code;
        uslc_test_free(&c);
        USLC_EXPECT(!ok);
        USLC_EXPECT(status == 4);
        USLC_EXPECT_STREQ("E4001", code);
    }
}
USLC_TEST(arena_failure_in_pipeline) {
    for (size_t failure = 0; failure < 20; ++failure) {
        UslcTestCompilation c;
        uslc_test_init(&c, "show 4@({x:x>3}U{y:y<0}).");
        c.value_arena.fail_after = failure;
        bool ok = uslc_test_compile(&c, 4);
        int status = c.d.exit_code;
        uslc_test_free(&c);
        USLC_EXPECT(ok || status == 70);
    }
}
USLC_TEST(set_membership_properties) {
    for (int n = -3; n <= 7; ++n) {
        char source[256];
        (void)snprintf(source, sizeof source,
                       "let int a be 0-%d.let set s be {x:x>0}U{x:x<0}.show a@(s I {y:y<5}).",
                       n + 3);
        UslcTestCompilation c;
        uslc_test_init(&c, source);
        bool ok = uslc_test_compile(&c, 4);
        bool correct = ok && c.result->boolean == (-(n + 3) != 0 && -(n + 3) < 5);
        uslc_test_free(&c);
        USLC_EXPECT(correct);
    }
}
USLC_TEST(display_nested_capture) {
    UslcTestCompilation c;
    uslc_test_init(&c, "show {a:2@{b:b>a}}U{b:b<1}.");
    USLC_EXPECT(uslc_test_compile(&c, 4));
    char *s = uslc_value_display(c.result, &c.d);
    bool correct = s && !strcmp(s, "{ x : ((2 @ { xa : (xa > x) }) | (x < 1)) }");
    free(s);
    uslc_test_free(&c);
    USLC_EXPECT(correct);
}
USLC_TEST(evaluator_invariants) {
    UslcTestCompilation c;
    uslc_test_init(&c, "show {x:x>1}.");
    USLC_EXPECT(uslc_test_compile(&c, 3));
    USLC_EXPECT(!uslc_evaluate(c.ast, 0, &c.value_arena, &c.result, &c.d));
    USLC_EXPECT_STREQ("E4090", c.d.code);
    uslc_test_free(&c);
    uslc_test_init(&c, "show 1+2.");
    USLC_EXPECT(uslc_test_compile(&c, 3));
    c.ast->right->left->token.kind = USLC_UNION;
    USLC_EXPECT(!uslc_evaluate(c.ast, c.binding_count, &c.value_arena, &c.result, &c.d));
    USLC_EXPECT_STREQ("E4090", c.d.code);
    uslc_test_free(&c);
}
void uslc_test_evaluator(void) {
    USLC_RUN(integer_limits);
    USLC_RUN(evaluation_cases);
    USLC_RUN(evaluation_errors);
    USLC_RUN(arena_failure_in_pipeline);
    USLC_RUN(set_membership_properties);
    USLC_RUN(display_nested_capture);
    USLC_RUN(evaluator_invariants);
}
