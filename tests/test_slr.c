#include "test.h"
USLC_TEST(core_closure_goto_and_states) {
    UslcGrammar g;
    UslcDiagnostics d = {0};
    UslcSlr slr = {0};
    uslc_grammar_core(&g);
    USLC_EXPECT(uslc_grammar_analyze(&g, &d));
    USLC_EXPECT(uslc_slr_build(&slr, &g, &d));
    USLC_EXPECT(uslc_items_has(&slr.states[0].items, (UslcItem){0, 0}));
    USLC_EXPECT(uslc_items_has(&slr.states[0].items, (UslcItem){5, 0}));
    USLC_EXPECT(uslc_items_has(&slr.states[0].items, (UslcItem){31, 0}));
    UslcItemSet accept = uslc_slr_goto(&g, &slr.states[0].items, USLC_S);
    USLC_EXPECT(uslc_items_has(&accept, (UslcItem){0, 1}));
    for (size_t i = 0; i < slr.count; ++i)
        for (size_t j = 0; j < i; ++j)
            USLC_EXPECT(memcmp(&slr.states[i].items, &slr.states[j].items, sizeof accept) != 0);
    USLC_EXPECT(slr.count == 66);
    uslc_slr_free(&slr);
}
USLC_TEST(conflicts) {
    UslcGrammar g = {.terminals = 3,
                     .symbols = 5,
                     .count = 3,
                     .eof = 2,
                     .names = {"a", "+", "$", "S'", "S"},
                     .productions = {{3, 1, {4}}, {4, 3, {4, 1, 4}}, {4, 1, {0}}}};
    UslcDiagnostics d = {0};
    UslcSlr slr = {0};
    USLC_EXPECT(uslc_grammar_analyze(&g, &d));
    USLC_EXPECT(!uslc_slr_build(&slr, &g, &d));
    USLC_EXPECT_STREQ("P2090", d.code);
    uslc_slr_free(&slr);
    uslc_diagnostics_init(&d);
    g = (UslcGrammar){
        .terminals = 2,
        .symbols = 6,
        .count = 5,
        .eof = 1,
        .names = {"a", "$", "S'", "S", "A", "B"},
        .productions = {{2, 1, {3}}, {3, 1, {4}}, {3, 1, {5}}, {4, 1, {0}}, {5, 1, {0}}}};
    USLC_EXPECT(uslc_grammar_analyze(&g, &d));
    USLC_EXPECT(!uslc_slr_build(&slr, &g, &d));
    USLC_EXPECT_STREQ("P2090", d.code);
    uslc_slr_free(&slr);
}
USLC_TEST(course_table_isomorphism) {
    FILE *file = fopen("tests/fixtures/slr-reference.csv", "rb");
    USLC_EXPECT(file != NULL);
    char line[4096], cells[66][41][16] = {{{0}}};
    USLC_EXPECT(fgets(line, sizeof line, file) != NULL);
    USLC_EXPECT(fgets(line, sizeof line, file) != NULL);
    size_t rows = 0;
    while (fgets(line, sizeof line, file)) {
        USLC_EXPECT(rows < 66);
        char *cursor = strchr(line, ',');
        USLC_EXPECT(cursor != NULL);
        ++cursor;
        for (size_t col = 0; col < 41; ++col) {
            size_t n = strcspn(cursor, ",\r\n");
            USLC_EXPECT(n < 16);
            memcpy(cells[rows][col], cursor, n);
            cursor += n;
            if (*cursor == ',')
                ++cursor;
        }
        ++rows;
    }
    fclose(file);
    USLC_EXPECT(rows == 66);
    UslcGrammar g;
    UslcDiagnostics d = {0};
    UslcSlr slr = {0};
    uslc_grammar_core(&g);
    USLC_EXPECT(uslc_grammar_analyze(&g, &d));
    USLC_EXPECT(uslc_slr_build(&slr, &g, &d));
    size_t mapping[66], reverse[66];
    for (size_t i = 0; i < 66; ++i)
        mapping[i] = reverse[i] = SIZE_MAX;
    mapping[0] = reverse[0] = 0;
    bool changed;
    do {
        changed = false;
        for (size_t original = 0; original < 66; ++original)
            if (mapping[original] != SIZE_MAX)
                for (size_t col = 0; col < 41; ++col) {
                    const char *cell = cells[original][col];
                    if ((col < 26 && cell[0] != 's') || !*cell)
                        continue;
                    size_t target = (size_t)strtoul(cell + (col < 26 ? 1 : 0), NULL, 10);
                    size_t actual = slr.states[mapping[original]].transitions[col];
                    USLC_EXPECT(target < 66 && actual < 66);
                    if (mapping[target] == SIZE_MAX) {
                        USLC_EXPECT(reverse[actual] == SIZE_MAX);
                        mapping[target] = actual;
                        reverse[actual] = target;
                        changed = true;
                    } else
                        USLC_EXPECT(mapping[target] == actual);
                }
    } while (changed);
    for (size_t original = 0; original < 66; ++original) {
        USLC_EXPECT(mapping[original] != SIZE_MAX);
        for (size_t col = 0; col < 41; ++col) {
            const char *cell = cells[original][col];
            const UslcState *state = &slr.states[mapping[original]];
            if (col >= 26) {
                USLC_EXPECT((*cell != 0) == (state->transitions[col] != SIZE_MAX));
                continue;
            }
            UslcAction a = state->actions[col];
            if (!*cell)
                USLC_EXPECT(a.kind == USLC_ACTION_ERROR);
            else if (!strcmp(cell, "acc"))
                USLC_EXPECT(a.kind == USLC_ACCEPT);
            else if (*cell == 's')
                USLC_EXPECT(a.kind == USLC_SHIFT &&
                            a.target == mapping[strtoul(cell + 1, NULL, 10)]);
            else
                USLC_EXPECT(*cell == 'r' && a.kind == USLC_REDUCE &&
                            a.target == strtoul(cell + 1, NULL, 10));
        }
    }
    uslc_slr_free(&slr);
}
void uslc_test_slr(void) {
    USLC_RUN(core_closure_goto_and_states);
    USLC_RUN(conflicts);
    USLC_RUN(course_table_isomorphism);
}
