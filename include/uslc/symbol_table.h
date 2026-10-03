#ifndef USLC_SYMBOL_TABLE_H
#define USLC_SYMBOL_TABLE_H
#include "uslc/ast.h"
typedef struct { const UslcParseNode *name; UslcType type; size_t binding;
    const UslcValue *value; } UslcSymbol;
/* Owns items, borrows names and values from the context. */
typedef struct { UslcSymbol *items; size_t count, capacity; } UslcSymbolTable;
void uslc_symbols_init(UslcSymbolTable *table);
void uslc_symbols_free(UslcSymbolTable *table);
bool uslc_name_equal(const UslcParseContext *context, const UslcParseNode *a,
                     const UslcParseNode *b);
UslcSymbol *uslc_symbol_find(UslcParseContext *context, const UslcParseNode *name);
bool uslc_symbol_add(UslcParseContext *context, UslcSymbol symbol);
#endif
