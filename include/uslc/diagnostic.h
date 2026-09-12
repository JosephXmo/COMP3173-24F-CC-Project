#ifndef USLC_DIAGNOSTIC_H
#define USLC_DIAGNOSTIC_H
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t line, column, offset;
} UslcPosition;
typedef struct {
    UslcPosition start, end;
} UslcSpan;
/* Owns the first diagnostic; no separately allocated strings. */
typedef struct {
    int exit_code;
    const char *code, *phase;
    UslcSpan span;
    char message[1024];
} UslcDiagnostics;
void uslc_diagnostics_init(UslcDiagnostics *d);
bool uslc_diagnose(UslcDiagnostics *d, int status, const char *code, const char *phase,
                   UslcSpan span, const char *format, ...);
bool uslc_oom(UslcDiagnostics *d);
bool uslc_depth(UslcDiagnostics *d, size_t depth, UslcSpan span);
#endif
