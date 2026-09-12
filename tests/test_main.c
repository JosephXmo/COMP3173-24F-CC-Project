#include "test.h"
size_t uslc_test_count, uslc_test_failures;
const char *uslc_test_name;
void uslc_test_init(UslcTestCompilation *c, const char *text) {
    *c = (UslcTestCompilation){0};
    c->source = (UslcSource){"test.set", (char *)text, strlen(text)};
    uslc_arena_init(&c->parse_arena);
    uslc_arena_init(&c->ast_arena);
    uslc_arena_init(&c->value_arena);
}
void uslc_test_free(UslcTestCompilation *c) {
    uslc_symbols_free(&c->symbols);
    uslc_tokens_free(&c->tokens);
    uslc_slr_free(&c->slr);
    uslc_arena_free(&c->parse_arena);
    uslc_arena_free(&c->ast_arena);
    uslc_arena_free(&c->value_arena);
}
bool uslc_test_compile(UslcTestCompilation *c, int phase) {
    if (!uslc_lex(&c->tokens, &c->source, &c->d))
        return false;
    if (phase == 1)
        return true;
    uslc_grammar_core(&c->grammar);
    if (!uslc_grammar_analyze(&c->grammar, &c->d) || !uslc_slr_build(&c->slr, &c->grammar, &c->d) ||
        !uslc_parse(&c->slr, &c->tokens, &c->parse_arena, &c->parse, &c->node_count, &c->d, NULL,
                    NULL) ||
        !uslc_ast_lower(c->parse, &c->ast_arena, &c->ast, &c->d))
        return false;
    if (phase == 2)
        return true;
    if (!uslc_type_check(c->ast, &c->source, &c->symbols, &c->binding_count, &c->d))
        return false;
    if (phase == 3)
        return true;
    return uslc_evaluate(c->ast, c->binding_count, &c->value_arena, &c->result, &c->d);
}
static int compare_files(const char *expected, const char *actual, bool text_mode) {
    FILE *a = fopen(expected, "rb"), *b = fopen(actual, "rb");
    if (!a || !b) {
        if (a)
            fclose(a);
        if (b)
            fclose(b);
        fprintf(stderr, "cannot compare %s and %s\n", expected, actual);
        return 1;
    }
    size_t offset = 0;
    int result = 0;
    for (;;) {
        int x = fgetc(a), y = fgetc(b);
#ifdef _WIN32
        if (text_mode && x == '\n' && y == '\r') {
            ++offset;
            y = fgetc(b);
        }
#else
        (void)text_mode;
#endif
        if (x != y) {
            fprintf(stderr, "difference at byte %zu: %s versus %s (%d / %d)\n", offset, expected,
                    actual, x, y);
            result = 1;
            break;
        }
        if (x == EOF)
            break;
        ++offset;
    }
    if (ferror(a) || ferror(b))
        result = 1;
    fclose(a);
    fclose(b);
    return result;
}
int main(int argc, char **argv) {
    if (argc == 4 && (!strcmp(argv[1], "--compare") || !strcmp(argv[1], "--compare-text")))
        return compare_files(argv[2], argv[3], !strcmp(argv[1], "--compare-text"));
    uslc_test_arena();
    uslc_test_lexer();
    uslc_test_grammar();
    uslc_test_slr();
    uslc_test_parser();
    uslc_test_type_checker();
    uslc_test_evaluator();
    uslc_test_json_writer();
    printf("%zu tests, %zu failures\n", uslc_test_count, uslc_test_failures);
    return uslc_test_failures ? 1 : 0;
}
