#include "uslc/grammar.h"
#include <string.h>

void uslc_grammar_core(UslcGrammar *g) {
    *g = (UslcGrammar){.terminals = 26, .symbols = 41, .count = 34, .eof = USLC_EOF};
    for (size_t i = 0; i < 26; ++i)
        g->names[i] = uslc_token_spelling((UslcTokenKind)i);
    static const char *const names[] = {"S'", "S", "D'", "D",   "T", "E", "E'", "E''",
                                        "Z",  "P", "P'", "P''", "R", "C", "A"};
    for (size_t i = 0; i < 15; ++i)
        g->names[26 + i] = names[i];
    static const UslcProduction productions[] = {
        {USLC_AUGMENTED, 1, {USLC_S}},
        {USLC_S, 3, {USLC_DECLS, USLC_C, USLC_DOT}},
        {USLC_S, 2, {USLC_C, USLC_DOT}},
        {USLC_DECLS, 2, {USLC_D, USLC_DECLS}},
        {USLC_DECLS, 1, {USLC_D}},
        {USLC_D, 6, {USLC_LET, USLC_T, USLC_ID, USLC_BE, USLC_E, USLC_DOT}},
        {USLC_T, 1, {USLC_INT}},
        {USLC_T, 1, {USLC_SET}},
        {USLC_E, 1, {USLC_TERM}},
        {USLC_E, 3, {USLC_E, USLC_UNION, USLC_TERM}},
        {USLC_E, 3, {USLC_E, USLC_PLUS, USLC_TERM}},
        {USLC_E, 3, {USLC_E, USLC_MINUS, USLC_TERM}},
        {USLC_TERM, 1, {USLC_FACTOR}},
        {USLC_TERM, 3, {USLC_TERM, USLC_INTERSECTION, USLC_FACTOR}},
        {USLC_TERM, 3, {USLC_TERM, USLC_STAR, USLC_FACTOR}},
        {USLC_FACTOR, 1, {USLC_NUM}},
        {USLC_FACTOR, 1, {USLC_ID}},
        {USLC_FACTOR, 3, {USLC_LPAREN, USLC_E, USLC_RPAREN}},
        {USLC_FACTOR, 4, {USLC_LBRACE, USLC_Z, USLC_P, USLC_RBRACE}},
        {USLC_Z, 2, {USLC_ID, USLC_COLON}},
        {USLC_P, 3, {USLC_P, USLC_OR, USLC_CONJ}},
        {USLC_P, 1, {USLC_CONJ}},
        {USLC_CONJ, 3, {USLC_CONJ, USLC_AND, USLC_PRED}},
        {USLC_CONJ, 1, {USLC_PRED}},
        {USLC_PRED, 1, {USLC_R}},
        {USLC_PRED, 3, {USLC_LPAREN, USLC_P, USLC_RPAREN}},
        {USLC_PRED, 2, {USLC_NOT, USLC_R}},
        {USLC_R, 3, {USLC_E, USLC_LT, USLC_E}},
        {USLC_R, 3, {USLC_E, USLC_GT, USLC_E}},
        {USLC_R, 3, {USLC_E, USLC_EQ, USLC_E}},
        {USLC_R, 3, {USLC_E, USLC_MEMBER, USLC_E}},
        {USLC_C, 2, {USLC_SHOW, USLC_A}},
        {USLC_A, 1, {USLC_E}},
        {USLC_A, 1, {USLC_P}}};
    memcpy(g->productions, productions, sizeof productions);
}
bool uslc_grammar_analyze(UslcGrammar *g, UslcDiagnostics *d) {
    if (!g->terminals || g->terminals > 32 || g->symbols > USLC_MAX_SYMBOLS ||
        g->symbols <= g->terminals || !g->count || g->count > USLC_MAX_PRODUCTIONS ||
        g->eof >= g->terminals)
        return uslc_diagnose(d, 70, "I9002", "internal", (UslcSpan){0},
                             "invalid grammar dimensions");
    for (size_t i = 0; i < g->symbols; ++i)
        if (!g->names[i])
            return uslc_diagnose(d, 70, "I9002", "internal", (UslcSpan){0},
                                 "missing grammar symbol name");
    for (size_t i = 0; i < g->count; ++i) {
        UslcProduction *p = &g->productions[i];
        if (p->lhs < g->terminals || p->lhs >= g->symbols || p->length > USLC_MAX_RHS)
            return uslc_diagnose(d, 70, "I9002", "internal", (UslcSpan){0},
                                 "invalid production %zu", i);
        for (size_t j = 0; j < p->length; ++j)
            if (p->rhs[j] >= g->symbols || p->rhs[j] == g->productions[0].lhs)
                return uslc_diagnose(d, 70, "I9002", "internal", (UslcSpan){0},
                                     "invalid RHS in production %zu", i);
        if (i && p->lhs == g->productions[0].lhs)
            return uslc_diagnose(d, 70, "I9002", "internal", (UslcSpan){0},
                                 "augmented start must have one production");
    }
    if (g->productions[0].length != 1 || g->productions[0].rhs[0] < g->terminals)
        return uslc_diagnose(d, 70, "I9002", "internal", (UslcSpan){0},
                             "invalid augmented start production");
    memset(g->nullable, 0, sizeof g->nullable);
    memset(g->first, 0, sizeof g->first);
    memset(g->follow, 0, sizeof g->follow);
    for (size_t i = 0; i < g->terminals; ++i)
        g->first[i] = UINT64_C(1) << i;
    bool changed;
    do {
        changed = false;
        for (size_t i = 0; i < g->count; ++i) {
            UslcProduction *p = &g->productions[i];
            uint64_t before = g->first[p->lhs];
            bool nullable = true;
            for (size_t j = 0; j < p->length; ++j) {
                g->first[p->lhs] |= g->first[p->rhs[j]];
                if (!g->nullable[p->rhs[j]]) {
                    nullable = false;
                    break;
                }
            }
            if (nullable && !g->nullable[p->lhs]) {
                g->nullable[p->lhs] = true;
                changed = true;
            }
            if (before != g->first[p->lhs])
                changed = true;
        }
    } while (changed);
    g->follow[g->productions[0].lhs] = UINT64_C(1) << g->eof;
    do {
        changed = false;
        for (size_t i = 0; i < g->count; ++i) {
            UslcProduction *p = &g->productions[i];
            uint64_t trailer = g->follow[p->lhs];
            for (size_t j = p->length; j > 0; --j) {
                size_t symbol = p->rhs[j - 1];
                if (symbol >= g->terminals) {
                    uint64_t before = g->follow[symbol];
                    g->follow[symbol] |= trailer;
                    if (before != g->follow[symbol])
                        changed = true;
                }
                trailer = g->nullable[symbol] ? trailer | g->first[symbol] : g->first[symbol];
            }
        }
    } while (changed);
    return true;
}
