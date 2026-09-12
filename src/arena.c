#include "uslc/arena.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct UslcArenaBlock {
    UslcArenaBlock *next;
    size_t used, capacity;
    UslcMaxAlign data[];
};
void uslc_arena_init(UslcArena *a) {
    *a = (UslcArena){.fail_after = SIZE_MAX};
}
void uslc_arena_free(UslcArena *a) {
    while (a->head) {
        UslcArenaBlock *next = a->head->next;
        free(a->head);
        a->head = next;
    }
    uslc_arena_init(a);
}
void *uslc_arena_alloc(UslcArena *a, size_t size, UslcDiagnostics *d) {
    size_t align = _Alignof(UslcMaxAlign);
    if (a->allocations == a->fail_after || size > SIZE_MAX - (align - 1)) {
        uslc_oom(d);
        return NULL;
    }
    if (!size)
        size = 1;
    size_t rounded = (size + align - 1) / align * align;
    UslcArenaBlock *b = a->head;
    if (!b || rounded > b->capacity - b->used) {
        size_t capacity = rounded > 4096 ? rounded : 4096;
        if (capacity > SIZE_MAX - sizeof *b) {
            uslc_oom(d);
            return NULL;
        }
        b = malloc(sizeof *b + capacity);
        if (!b) {
            uslc_oom(d);
            return NULL;
        }
        b->next = a->head;
        b->used = 0;
        b->capacity = capacity;
        a->head = b;
    }
    void *p = (unsigned char *)b->data + b->used;
    b->used += rounded;
    ++a->allocations;
    memset(p, 0, size);
    return p;
}
bool uslc_reserve(void **data, size_t *capacity, size_t count, size_t element_size,
                  UslcDiagnostics *d) {
    if (count <= *capacity)
        return true;
    if (!element_size || count > SIZE_MAX / element_size)
        return uslc_oom(d);
    size_t next = *capacity ? *capacity : 16;
    while (next < count) {
        if (next > SIZE_MAX / 2) {
            next = count;
            break;
        }
        next *= 2;
    }
    if (next > SIZE_MAX / element_size)
        next = count;
    void *p = realloc(*data, next * element_size);
    if (!p)
        return uslc_oom(d);
    *data = p;
    *capacity = next;
    return true;
}
