#ifndef USLC_SOURCE_H
#define USLC_SOURCE_H
#include <stdio.h>
#include "uslc/diagnostic.h"
/* Owns data; name is borrowed from the driver. Length excludes the trailing NUL. */
typedef struct {
    const char *name;
    char *data;
    size_t length;
} UslcSource;
bool uslc_source_read(UslcSource *s, FILE *input, const char *name, UslcDiagnostics *d);
void uslc_source_free(UslcSource *s);
#endif
