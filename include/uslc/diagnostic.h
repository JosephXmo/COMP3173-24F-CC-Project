#ifndef USLC_DIAGNOSTIC_H
#define USLC_DIAGNOSTIC_H
#include <stdbool.h>
#include <stddef.h>

typedef struct { size_t line, column, offset; } UslcPosition;
typedef struct { UslcPosition start, end; } UslcSpan;
typedef enum { USLC_OK=0, USLC_LEXICAL=1, USLC_SYNTAX=2, USLC_TYPING=3,
    USLC_EVALUATION=4, USLC_USAGE=64, USLC_INPUT=66, USLC_INTERNAL=70,
    USLC_OUTPUT=74 } UslcStatus;
/* Owns fixed-size diagnostic text; records only the first failure. */
typedef struct {
    UslcStatus status;
    const char *code, *phase;
    UslcSpan span;
    char message[256], note[512];
} UslcDiagnostics;
void uslc_diagnostics_init(UslcDiagnostics *d);
void uslc_diagnostics_free(UslcDiagnostics *d);
void uslc_diagnose(UslcDiagnostics *d, UslcStatus status, const char *code,
                   UslcSpan span, const char *message);
UslcSpan uslc_span_origin(void);
#endif
