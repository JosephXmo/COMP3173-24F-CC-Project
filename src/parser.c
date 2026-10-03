#include "uslc/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    size_t state;
    UslcParseNode *node;
} StackEntry;
bool uslc_parse(const UslcSlr *slr, const UslcTokens *tokens, UslcArena *arena,
                UslcParseNode **root, size_t *node_count, UslcDiagnostics *d, UslcTrace trace,
                void *context) {
    StackEntry *stack = NULL;
    size_t capacity = 0, count = 1, cursor = 0;
    bool success = false;
    *root = NULL;
    *node_count = 0;
    void *data = stack;
    if (!uslc_reserve(&data, &capacity, 1, sizeof *stack, d))
        return false;
    stack = data;
    stack[0] = (StackEntry){0};
    while (cursor < tokens->count) {
        size_t state = stack[count - 1].state;
        UslcToken t = tokens->data[cursor];
        if ((size_t)t.kind >= slr->grammar->terminals)
            break;
        UslcAction a = slr->states[state].actions[t.kind];
        if (trace)
            trace(context, state, t, a);
        if (a.kind == USLC_ACCEPT) {
            *root = stack[count - 1].node;
            success = true;
            break;
        }
        if (a.kind == USLC_ACTION_ERROR) {
            char expected[768] = "";
            size_t used = 0;
            for (size_t i = 0; i < slr->grammar->terminals; ++i)
                if (slr->states[state].actions[i].kind) {
                    int n = snprintf(expected + used, sizeof expected - used, "%s%s",
                                     used ? ", " : "", slr->grammar->names[i]);
                    if (n < 0 || (size_t)n >= sizeof expected - used)
                        break;
                    used += (size_t)n;
                }
            uslc_diagnose(d, 2, t.kind == USLC_EOF ? "P2002" : "P2001", "syntax", t.span,
                          "unexpected %s; expected: %s", uslc_token_name(t.kind), expected);
            break;
        }
        UslcParseNode *node = uslc_arena_alloc(arena, sizeof *node, d);
        if (!node)
            break;
        ++*node_count;
        if (a.kind == USLC_SHIFT) {
            node->symbol = (size_t)t.kind;
            node->production = SIZE_MAX;
            node->token = t;
            node->span = t.span;
            node->height = 1;
            ++cursor;
        } else {
            const UslcProduction *p = &slr->grammar->productions[a.target];
            if (p->length >= count)
                break;
            node->symbol = p->lhs;
            node->production = a.target;
            node->count = p->length;
            node->height = 1;
            for (size_t j = 0; j < p->length; ++j) {
                node->children[j] = stack[count - p->length + j].node;
                if (node->children[j]->height >= node->height)
                    node->height = node->children[j]->height + 1;
            }
            node->span = p->length ? (UslcSpan){node->children[0]->span.start,
                                                node->children[p->length - 1]->span.end}
                                   : (UslcSpan){t.span.start, t.span.start};
            if (!uslc_depth(d, node->height, node->span))
                break;
            count -= p->length;
            a.target = slr->states[stack[count - 1].state].transitions[p->lhs];
            if (a.target == SIZE_MAX)
                break;
        }
        data = stack;
        if (count == SIZE_MAX || !uslc_reserve(&data, &capacity, count + 1, sizeof *stack, d))
            break;
        stack = data;
        stack[count++] = (StackEntry){a.target, node};
    }
    free(stack);
    if (!success && !d->exit_code)
        uslc_diagnose(d, 70, "I9003", "internal", (UslcSpan){0},
                      "invalid parser state or token stream");
    return success;
}
