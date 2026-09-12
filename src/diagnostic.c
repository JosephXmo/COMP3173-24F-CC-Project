#include "uslc/diagnostic.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void uslc_diagnostics_init(UslcDiagnostics *d) {
    memset(d, 0, sizeof *d);
}
bool uslc_diagnose(UslcDiagnostics *d, int status, const char *code, const char *phase,
                   UslcSpan span, const char *format, ...) {
    if (!d->exit_code) {
        d->exit_code = status;
        d->code = code;
        d->phase = phase;
        d->span = span;
        va_list args;
        va_start(args, format);
        (void)vsnprintf(d->message, sizeof d->message, format, args);
        va_end(args);
    }
    return false;
}
bool uslc_oom(UslcDiagnostics *d) {
    return uslc_diagnose(d, 70, "I9001", "internal", (UslcSpan){0},
                         "allocation or size limit exceeded");
}
bool uslc_depth(UslcDiagnostics *d, size_t depth, UslcSpan span) {
    return depth < 256 || uslc_diagnose(d, 70, "I9004", "internal", span,
                                        "tree nesting exceeds the implementation limit (256)");
}
