#include "uslc/source.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "uslc/arena.h"
void uslc_source_free(UslcSource *s) { free(s->data); *s=(UslcSource){0}; }
bool uslc_source_copy(UslcSource *s, const char *bytes, size_t length, const char *path, UslcDiagnostics *d) {
    size_t capacity=0;
    *s=(UslcSource){.path=path};
    void *data=NULL;
    if (length==SIZE_MAX || !uslc_grow(&data, &capacity, length+1, 1, d)) {
        uslc_diagnose(d, USLC_INTERNAL, "I9001", uslc_span_origin(), "source too large"); return false;
    }
    s->data=data;
    memcpy(s->data, bytes, length); s->data[length]='\0'; s->length=length; return true;
}
bool uslc_source_read(UslcSource *s, FILE *file, const char *path, UslcDiagnostics *d) {
    size_t capacity=0; *s=(UslcSource){.path=path};
    for (;;) {
        void *data=s->data;
        if (s->length>SIZE_MAX-4097 || !uslc_grow(&data, &capacity, s->length+4097, 1, d)) {
            uslc_diagnose(d, USLC_INTERNAL, "I9001", uslc_span_origin(), "source too large"); return false;
        }
        s->data=data;
        size_t n=fread(s->data+s->length, 1, 4096, file); s->length+=n;
        if (n<4096) {
            if (ferror(file)) { uslc_diagnose(d, USLC_INPUT, "F6001", uslc_span_origin(), "cannot read input"); return false; }
            break;
        }
    }
    s->data[s->length]='\0'; return true;
}
