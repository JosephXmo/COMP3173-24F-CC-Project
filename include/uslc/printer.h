#ifndef USLC_PRINTER_H
#define USLC_PRINTER_H
#include "uslc/evaluator.h"
#include "uslc/json_writer.h"
#include "uslc/symbol_table.h"
/* Returned display string belongs to caller and must be freed with free(). */
char *uslc_value_display(const UslcValue *value, UslcDiagnostics *d);
bool uslc_print_grammar(FILE *output, const UslcGrammar *g);
bool uslc_print_slr(FILE *output, const UslcSlr *slr);
void uslc_print_trace(void *context, size_t state, UslcToken token, UslcAction action);
bool uslc_print_report(FILE *output, const UslcSource *source, const UslcDiagnostics *d,
                       int completed, size_t token_count, size_t node_count, const UslcValue *value,
                       const char *display);
void uslc_json_tokens(UslcJsonWriter *w, const UslcSource *source, const UslcTokens *tokens);
void uslc_json_parse_tree(UslcJsonWriter *w, const UslcSource *source, const UslcGrammar *g,
                          const UslcParseNode *root);
void uslc_json_ast(UslcJsonWriter *w, const UslcSource *source, const UslcAst *root,
                   const UslcSymbolTable *symbols);
void uslc_json_evaluation(UslcJsonWriter *w, const UslcValue *value, const char *display);
void uslc_json_diagnostics(UslcJsonWriter *w, const UslcSource *source, const UslcDiagnostics *d);
#endif
