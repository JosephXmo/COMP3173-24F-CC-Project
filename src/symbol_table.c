#include "uslc/symbol_table.h"
#include <stdlib.h>
#include <string.h>
#include "uslc/parse_context.h"
void uslc_symbols_init(UslcSymbolTable *t) { *t=(UslcSymbolTable){0}; }
void uslc_symbols_free(UslcSymbolTable *t) { free(t->items); uslc_symbols_init(t); }
bool uslc_name_equal(const UslcParseContext *c, const UslcParseNode *a, const UslcParseNode *b) {
    size_t n=a->span.end.offset-a->span.start.offset;
    return n==b->span.end.offset-b->span.start.offset &&
        memcmp(c->source.data+a->span.start.offset,c->source.data+b->span.start.offset,n)==0;
}
UslcSymbol *uslc_symbol_find(UslcParseContext *c, const UslcParseNode *name) {
    for (size_t i=c->symbols.count;i>0;--i)
        if (uslc_name_equal(c,c->symbols.items[i-1].name,name)) return &c->symbols.items[i-1];
    return NULL;
}
bool uslc_symbol_add(UslcParseContext *c, UslcSymbol symbol) {
    void *items=c->symbols.items;
    if (!uslc_grow(&items,&c->symbols.capacity,c->symbols.count+1,sizeof symbol,&c->diagnostics)) return false;
    c->symbols.items=items;
    c->symbols.items[c->symbols.count++]=symbol; return true;
}
