#include "uslc/slr.h"
#include <stdlib.h>
#include <string.h>
#include "uslc/arena.h"
bool uslc_items_has(const UslcItemSet *set, UslcItem item) {
    size_t bit = item.production * 9 + item.dot;
    return (set->words[bit / 64] & (UINT64_C(1) << (bit % 64))) != 0;
}
void uslc_items_add(UslcItemSet *set, UslcItem item) {
    size_t bit = item.production * 9 + item.dot;
    set->words[bit / 64] |= UINT64_C(1) << (bit % 64);
}
void uslc_slr_closure(const UslcGrammar *g, UslcItemSet *set) {
    bool changed;
    do {
        changed = false;
        for (size_t p = 0; p < g->count; ++p)
            for (size_t dot = 0; dot < g->productions[p].length; ++dot) {
                if (!uslc_items_has(set, (UslcItem){p, dot}))
                    continue;
                size_t symbol = g->productions[p].rhs[dot];
                if (symbol < g->terminals)
                    continue;
                for (size_t q = 0; q < g->count; ++q)
                    if (g->productions[q].lhs == symbol && !uslc_items_has(set, (UslcItem){q, 0})) {
                        uslc_items_add(set, (UslcItem){q, 0});
                        changed = true;
                    }
            }
    } while (changed);
}
UslcItemSet uslc_slr_goto(const UslcGrammar *g, const UslcItemSet *set, size_t symbol) {
    UslcItemSet result = {0};
    for (size_t p = 0; p < g->count; ++p)
        for (size_t dot = 0; dot < g->productions[p].length; ++dot)
            if (g->productions[p].rhs[dot] == symbol && uslc_items_has(set, (UslcItem){p, dot}))
                uslc_items_add(&result, (UslcItem){p, dot + 1});
    uslc_slr_closure(g, &result);
    return result;
}
static bool append(UslcSlr *slr, UslcItemSet items, UslcDiagnostics *d) {
    void *data = slr->states;
    if (slr->count == SIZE_MAX ||
        !uslc_reserve(&data, &slr->capacity, slr->count + 1, sizeof *slr->states, d))
        return false;
    slr->states = data;
    UslcState *s = &slr->states[slr->count++];
    *s = (UslcState){.items = items};
    for (size_t i = 0; i < USLC_MAX_SYMBOLS; ++i)
        s->transitions[i] = SIZE_MAX;
    return true;
}
static bool action(UslcSlr *slr, size_t state, size_t terminal, UslcAction value,
                   UslcDiagnostics *d) {
    UslcAction *cell = &slr->states[state].actions[terminal];
    if (cell->kind && (cell->kind != value.kind || cell->target != value.target))
        return uslc_diagnose(d, 70, "P2090", "internal", (UslcSpan){0},
                             "grammar conflict in state %zu on '%s': %s/%s", state,
                             slr->grammar->names[terminal],
                             cell->kind == USLC_SHIFT ? "shift" : "reduce",
                             value.kind == USLC_SHIFT ? "shift" : "reduce");
    *cell = value;
    return true;
}
bool uslc_slr_build(UslcSlr *slr, const UslcGrammar *g, UslcDiagnostics *d) {
    slr->grammar = g;
    UslcItemSet start = {0}, empty = {0};
    uslc_items_add(&start, (UslcItem){0, 0});
    uslc_slr_closure(g, &start);
    if (!append(slr, start, d))
        return false;
    for (size_t i = 0; i < slr->count; ++i)
        for (size_t symbol = 0; symbol < g->symbols; ++symbol) {
            UslcItemSet next = uslc_slr_goto(g, &slr->states[i].items, symbol);
            if (!memcmp(&next, &empty, sizeof next))
                continue;
            size_t target = 0;
            while (target < slr->count && memcmp(&next, &slr->states[target].items, sizeof next))
                ++target;
            if (target == slr->count && !append(slr, next, d))
                return false;
            slr->states[i].transitions[symbol] = target;
            if (symbol < g->terminals &&
                !action(slr, i, symbol, (UslcAction){USLC_SHIFT, target}, d))
                return false;
        }
    for (size_t i = 0; i < slr->count; ++i)
        for (size_t p = 0; p < g->count; ++p) {
            if (!uslc_items_has(&slr->states[i].items, (UslcItem){p, g->productions[p].length}))
                continue;
            if (!p) {
                if (!action(slr, i, g->eof, (UslcAction){USLC_ACCEPT, 0}, d))
                    return false;
            } else
                for (size_t t = 0; t < g->terminals; ++t)
                    if ((g->follow[g->productions[p].lhs] & (UINT64_C(1) << t)) &&
                        !action(slr, i, t, (UslcAction){USLC_REDUCE, p}, d))
                        return false;
        }
    return true;
}
void uslc_slr_init(UslcSlr *slr) {
    *slr = (UslcSlr){0};
}
void uslc_slr_free(UslcSlr *slr) {
    free(slr->states);
    uslc_slr_init(slr);
}
