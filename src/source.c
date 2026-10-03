#include "uslc/source.h"
#include <stdint.h>
#include <stdlib.h>
#include "uslc/arena.h"

bool uslc_source_read(UslcSource *s, FILE *input, const char *name, UslcDiagnostics *d) {
    *s = (UslcSource){.name = name};
    size_t capacity = 0;
    for (;;) {
        void *data = s->data;
        if (s->length > SIZE_MAX - 4097 || !uslc_reserve(&data, &capacity, s->length + 4097, 1, d))
            return uslc_oom(d);
        s->data = data;
        size_t n = fread(s->data + s->length, 1, 4096, input);
        s->length += n;
        if (n < 4096) {
            if (ferror(input))
                return uslc_diagnose(d, 66, "I9066", "input", (UslcSpan){0}, "cannot read source");
            break;
        }
    }
    s->data[s->length] = '\0';
    return true;
}
void uslc_source_free(UslcSource *s) {
    free(s->data);
    *s = (UslcSource){0};
}
