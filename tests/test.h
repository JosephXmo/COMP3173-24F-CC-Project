#ifndef USLC_TEST_H
#define USLC_TEST_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uslc/printer.h"
#include "uslc/type_checker.h"
extern size_t uslc_test_count, uslc_test_failures;
extern const char *uslc_test_name;
#define USLC_TEST(name) static void name(void)
#define USLC_RUN(name)                                                                             \
    do {                                                                                           \
        uslc_test_name = #name;                                                                    \
        ++uslc_test_count;                                                                         \
        name();                                                                                    \
    } while (0)
#define USLC_EXPECT(condition)                                                                     \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL %s (%s:%d): %s\n", uslc_test_name, __FILE__, __LINE__,           \
                    #condition);                                                                   \
            ++uslc_test_failures;                                                                  \
            return;                                                                                \
        }                                                                                          \
    } while (0)
#define USLC_EXPECT_EQ_INT(expected, actual) USLC_EXPECT((expected) == (actual))
#define USLC_EXPECT_STREQ(expected, actual)                                                        \
    do {                                                                                           \
        const char *uslc_expected_ = (expected), *uslc_actual_ = (actual);                         \
        if (!uslc_actual_ || strcmp(uslc_expected_, uslc_actual_)) {                               \
            fprintf(stderr, "FAIL %s (%s:%d): expected <%s>, got <%s>\n", uslc_test_name,          \
                    __FILE__, __LINE__, uslc_expected_, uslc_actual_ ? uslc_actual_ : "NULL");     \
            ++uslc_test_failures;                                                                  \
            return;                                                                                \
        }                                                                                          \
    } while (0)
typedef struct {
    UslcSource source;
    UslcDiagnostics d;
    UslcTokens tokens;
    UslcGrammar grammar;
    UslcSlr slr;
    UslcArena parse_arena, ast_arena, value_arena;
    UslcParseNode *parse;
    UslcAst *ast;
    UslcSymbolTable symbols;
    const UslcValue *result;
    size_t node_count, binding_count;
} UslcTestCompilation;
void uslc_test_init(UslcTestCompilation *c, const char *text);
void uslc_test_free(UslcTestCompilation *c);
bool uslc_test_compile(UslcTestCompilation *c, int phase);
void uslc_test_arena(void);
void uslc_test_lexer(void);
void uslc_test_grammar(void);
void uslc_test_slr(void);
void uslc_test_parser(void);
void uslc_test_type_checker(void);
void uslc_test_evaluator(void);
void uslc_test_json_writer(void);
#endif
