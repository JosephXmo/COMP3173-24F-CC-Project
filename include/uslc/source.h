#ifndef USLC_SOURCE_H
#define USLC_SOURCE_H
#include <stdio.h>
#include "uslc/diagnostic.h"
/* Owns data; path is borrowed from the caller. No text-mode conversion. */
typedef struct { const char *path; char *data; size_t length; } UslcSource;
bool uslc_source_read(UslcSource *source, FILE *file, const char *path, UslcDiagnostics *d);
bool uslc_source_copy(UslcSource *source, const char *bytes, size_t length,
                      const char *path, UslcDiagnostics *d);
void uslc_source_free(UslcSource *source);
#endif
