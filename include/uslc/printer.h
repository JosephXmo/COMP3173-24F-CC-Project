#ifndef USLC_PRINTER_H
#define USLC_PRINTER_H
#include <stdio.h>
#include "uslc/evaluator.h"
/* Returned display string is caller-owned (free with free()). */
char *uslc_value_display(UslcParseContext *context, const UslcValue *value);
bool uslc_print_report(FILE *file, UslcParseContext *context);
#endif
