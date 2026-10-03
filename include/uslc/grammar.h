#ifndef USLC_GRAMMAR_H
#define USLC_GRAMMAR_H
#include <stdint.h>
#include "uslc/token.h"
#define USLC_MAX_SYMBOLS 64
#define USLC_MAX_PRODUCTIONS 64
#define USLC_MAX_RHS 8
typedef enum {
    USLC_AUGMENTED = 26,
    USLC_S,
    USLC_DECLS,
    USLC_D,
    USLC_T,
    USLC_E,
    USLC_TERM,
    USLC_FACTOR,
    USLC_Z,
    USLC_P,
    USLC_CONJ,
    USLC_PRED,
    USLC_R,
    USLC_C,
    USLC_A
} UslcNonterminal;
typedef struct {
    size_t lhs, length;
    size_t rhs[USLC_MAX_RHS];
} UslcProduction;
/* Fixed bounds are validated before algorithms access the grammar. Names are borrowed. */
typedef struct {
    size_t terminals, symbols, count, eof;
    const char *names[USLC_MAX_SYMBOLS];
    UslcProduction productions[USLC_MAX_PRODUCTIONS];
    bool nullable[USLC_MAX_SYMBOLS];
    uint64_t first[USLC_MAX_SYMBOLS], follow[USLC_MAX_SYMBOLS];
} UslcGrammar;
void uslc_grammar_core(UslcGrammar *g);
bool uslc_grammar_analyze(UslcGrammar *g, UslcDiagnostics *d);
#endif
