#ifndef USLC_SYMBOL_TABLE_H
#define USLC_SYMBOL_TABLE_H
#include "uslc/ast.h"
typedef struct {
    UslcToken name;
    UslcType type;
    size_t binding;
} UslcSymbol;
/* Owns entries; token names borrow source. Truncate count to leave a lexical scope. */
typedef struct {
    UslcSymbol *entries;
    size_t count, capacity;
} UslcSymbolTable;
void uslc_symbols_init(UslcSymbolTable *table);
void uslc_symbols_free(UslcSymbolTable *table);
bool uslc_symbols_push(UslcSymbolTable *table, UslcSymbol symbol, UslcDiagnostics *d);
bool uslc_symbols_find(const UslcSymbolTable *table, const UslcSource *source, UslcToken name,
                       UslcSymbol *result);
#endif
