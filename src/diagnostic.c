#include "uslc/diagnostic.h"
#include <stdio.h>
#include <string.h>
UslcSpan uslc_span_origin(void) { return (UslcSpan){{1,1,0},{1,1,0}}; }
void uslc_diagnostics_init(UslcDiagnostics *d) { memset(d, 0, sizeof *d); }
void uslc_diagnostics_free(UslcDiagnostics *d) { uslc_diagnostics_init(d); }
void uslc_diagnose(UslcDiagnostics *d, UslcStatus status, const char *code,
                   UslcSpan span, const char *message) {
    if (d->status != USLC_OK) return;
    d->status = status; d->code = code; d->span = span;
    switch (status) {
    case USLC_LEXICAL: d->phase="lexical"; break;
    case USLC_SYNTAX: d->phase="syntax"; break;
    case USLC_TYPING: d->phase="typing"; break;
    case USLC_EVALUATION: d->phase="evaluate"; break;
    case USLC_INPUT: d->phase="input"; break;
    case USLC_OUTPUT: d->phase="output"; break;
    case USLC_USAGE: d->phase="usage"; break;
    default: d->phase="internal"; break;
    }
    snprintf(d->message, sizeof d->message, "%s", message);
}
