#include "uslc/arena.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
struct UslcArenaBlock {
    UslcArenaBlock *next;
    size_t used, capacity;
    max_align_t data[];
};
void uslc_arena_init(UslcArena *a, UslcDiagnostics *d) { a->blocks=NULL; a->diagnostics=d; }
void uslc_arena_free(UslcArena *a) {
    while (a->blocks) { UslcArenaBlock *next=a->blocks->next; free(a->blocks); a->blocks=next; }
}
static void uslc_oom(UslcDiagnostics *d) {
    uslc_diagnose(d, USLC_INTERNAL, "I9001", uslc_span_origin(), "out of memory or allocation size overflow");
}
void *uslc_arena_alloc(UslcArena *a, size_t size) {
    size_t alignment=alignof(max_align_t);
    if (size > SIZE_MAX-(alignment-1)) { uslc_oom(a->diagnostics); return NULL; }
    size_t rounded=(size+alignment-1)/alignment*alignment;
    if (!rounded) rounded=alignment;
    UslcArenaBlock *b=a->blocks;
    if (!b || rounded > b->capacity-b->used) {
        size_t capacity=rounded > 16384 ? rounded : 16384;
        if (capacity > SIZE_MAX-sizeof *b) { uslc_oom(a->diagnostics); return NULL; }
        b=malloc(sizeof *b+capacity);
        if (!b) { uslc_oom(a->diagnostics); return NULL; }
        b->next=a->blocks; b->used=0; b->capacity=capacity; a->blocks=b;
    }
    void *result=(unsigned char *)b->data+b->used;
    b->used+=rounded;
    memset(result, 0, size);
    return result;
}
bool uslc_grow(void **data, size_t *capacity, size_t needed, size_t item_size, UslcDiagnostics *d) {
    if (needed<=*capacity) return true;
    if (!item_size || needed>SIZE_MAX/item_size) { uslc_oom(d); return false; }
    size_t next=*capacity ? *capacity : 16;
    while (next<needed) {
        if (next>SIZE_MAX/2) { next=needed; break; }
        next*=2;
    }
    if (next>SIZE_MAX/item_size) next=needed;
    void *p=realloc(*data, next*item_size);
    if (!p) { uslc_oom(d); return false; }
    *data=p; *capacity=next; return true;
}
