#ifndef USLC_ARENA_H
#define USLC_ARENA_H
#include <stddef.h>
#include "uslc/diagnostic.h"
typedef struct UslcArenaBlock UslcArenaBlock;
/* Owns all blocks; returned objects are borrowed until arena_free. */
typedef struct { UslcArenaBlock *blocks; UslcDiagnostics *diagnostics; } UslcArena;
void uslc_arena_init(UslcArena *arena, UslcDiagnostics *diagnostics);
void uslc_arena_free(UslcArena *arena);
void *uslc_arena_alloc(UslcArena *arena, size_t size);
bool uslc_grow(void **data, size_t *capacity, size_t needed, size_t item_size,
               UslcDiagnostics *diagnostics);
#endif
