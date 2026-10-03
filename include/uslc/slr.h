#ifndef USLC_SLR_H
#define USLC_SLR_H
#include "uslc/grammar.h"
#define USLC_ITEM_WORDS 9
typedef struct {
    size_t production, dot;
} UslcItem;
typedef struct {
    uint64_t words[USLC_ITEM_WORDS];
} UslcItemSet;
typedef enum { USLC_ACTION_ERROR, USLC_SHIFT, USLC_REDUCE, USLC_ACCEPT } UslcActionKind;
typedef struct {
    UslcActionKind kind;
    size_t target;
} UslcAction;
typedef struct {
    UslcItemSet items;
    size_t transitions[USLC_MAX_SYMBOLS]; /* SIZE_MAX means no edge. */
    UslcAction actions[32];
} UslcState;
/* Owns states; borrows the analyzed grammar. State order is BFS in symbol order. */
typedef struct {
    const UslcGrammar *grammar;
    UslcState *states;
    size_t count, capacity;
} UslcSlr;
void uslc_slr_init(UslcSlr *slr);
bool uslc_items_has(const UslcItemSet *set, UslcItem item);
void uslc_items_add(UslcItemSet *set, UslcItem item);
void uslc_slr_closure(const UslcGrammar *g, UslcItemSet *set);
UslcItemSet uslc_slr_goto(const UslcGrammar *g, const UslcItemSet *set, size_t symbol);
bool uslc_slr_build(UslcSlr *slr, const UslcGrammar *g, UslcDiagnostics *d);
void uslc_slr_free(UslcSlr *slr);
#endif
