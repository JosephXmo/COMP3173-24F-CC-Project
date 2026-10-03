#include "uslc/symbol_table.h"
#include <stdlib.h>
void uslc_symbols_init(UslcSymbolTable *table) {
    *table = (UslcSymbolTable){0};
}
void uslc_symbols_free(UslcSymbolTable *table) {
    free(table->entries);
    uslc_symbols_init(table);
}
bool uslc_symbols_push(UslcSymbolTable *table, UslcSymbol symbol, UslcDiagnostics *d) {
    void *data = table->entries;
    if (table->count == SIZE_MAX ||
        !uslc_reserve(&data, &table->capacity, table->count + 1, sizeof *table->entries, d))
        return false;
    table->entries = data;
    table->entries[table->count++] = symbol;
    return true;
}
bool uslc_symbols_find(const UslcSymbolTable *table, const UslcSource *source, UslcToken name,
                       UslcSymbol *result) {
    for (size_t i = table->count; i > 0; --i)
        if (uslc_token_equals(source, table->entries[i - 1].name, name)) {
            *result = table->entries[i - 1];
            return true;
        }
    return false;
}
