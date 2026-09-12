#ifndef USLC_ARENA_H
#define USLC_ARENA_H
#include "uslc/diagnostic.h"
#include <stdint.h>
#ifdef USLC_NO_MAX_ALIGN_T
/* MSVC's C headers omit max_align_t. Covers all value types used by this project. */
typedef union {
    long double floating;
    int64_t integer;
    void *pointer;
    void (*function)(void);
} UslcMaxAlign;
#else
typedef max_align_t UslcMaxAlign;
#endif
typedef struct UslcArenaBlock UslcArenaBlock;
/* Owns every block. Allocations are zeroed and aligned for all project value types. */
typedef struct {
    UslcArenaBlock *head;
    size_t allocations, fail_after;
} UslcArena;
void uslc_arena_init(UslcArena *a);
void uslc_arena_free(UslcArena *a);
void *uslc_arena_alloc(UslcArena *a, size_t size, UslcDiagnostics *d);
bool uslc_reserve(void **data, size_t *capacity, size_t count, size_t element_size,
                  UslcDiagnostics *d);
#endif
