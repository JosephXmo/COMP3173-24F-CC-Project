#ifndef USLC_JSON_WRITER_H
#define USLC_JSON_WRITER_H
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    char kind;
    bool nonempty, needs_value;
} UslcJsonFrame;
/* Borrows output. Sticky failure records stream errors and invalid writer calls. */
typedef struct {
    FILE *output;
    bool compact, failed, root_written;
    size_t depth;
    UslcJsonFrame frames[1024];
} UslcJsonWriter;
void uslc_json_init(UslcJsonWriter *w, FILE *output, bool compact);
void uslc_json_object(UslcJsonWriter *w);
void uslc_json_array(UslcJsonWriter *w);
void uslc_json_end(UslcJsonWriter *w);
void uslc_json_key(UslcJsonWriter *w, const char *key);
void uslc_json_string(UslcJsonWriter *w, const char *text);
void uslc_json_slice(UslcJsonWriter *w, const char *text, size_t length);
void uslc_json_integer(UslcJsonWriter *w, int64_t value);
void uslc_json_size(UslcJsonWriter *w, size_t value);
void uslc_json_boolean(UslcJsonWriter *w, bool value);
void uslc_json_null(UslcJsonWriter *w);
bool uslc_json_finish(UslcJsonWriter *w);
#endif
