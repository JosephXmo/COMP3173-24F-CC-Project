#ifndef USLC_TYPE_CHECKER_H
#define USLC_TYPE_CHECKER_H
#include "uslc/symbol_table.h"
bool uslc_type_check(UslcAst *program, const UslcSource *source, UslcSymbolTable *globals,
                     size_t *binding_count, UslcDiagnostics *d);
#endif
