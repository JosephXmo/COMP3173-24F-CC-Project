#include "uslc/json_writer.h"
#include <inttypes.h>
#include <string.h>
static void put(UslcJsonWriter *w, int c) {
    if (!w->failed && fputc(c, w->output) == EOF)
        w->failed = true;
}
static void raw(UslcJsonWriter *w, const char *s) {
    while (*s)
        put(w, (unsigned char)*s++);
}
static void newline(UslcJsonWriter *w) {
    if (w->compact)
        return;
    put(w, '\n');
    for (size_t i = 0; i < w->depth; ++i)
        raw(w, "  ");
}
static void quoted(UslcJsonWriter *w, const char *s, size_t n) {
    static const char hex[] = "0123456789abcdef";
    put(w, '"');
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"':
            raw(w, "\\\"");
            break;
        case '\\':
            raw(w, "\\\\");
            break;
        case '\b':
            raw(w, "\\b");
            break;
        case '\f':
            raw(w, "\\f");
            break;
        case '\n':
            raw(w, "\\n");
            break;
        case '\r':
            raw(w, "\\r");
            break;
        case '\t':
            raw(w, "\\t");
            break;
        default:
            if (c < 32) {
                raw(w, "\\u00");
                put(w, hex[c / 16]);
                put(w, hex[c % 16]);
            } else
                put(w, c);
            break;
        }
    }
    put(w, '"');
}
static bool before(UslcJsonWriter *w) {
    if (w->failed)
        return false;
    if (!w->depth) {
        if (w->root_written) {
            w->failed = true;
            return false;
        }
        w->root_written = true;
        return true;
    }
    UslcJsonFrame *f = &w->frames[w->depth - 1];
    if (f->kind == '{') {
        if (!f->needs_value) {
            w->failed = true;
            return false;
        }
        f->needs_value = false;
    } else {
        if (f->nonempty)
            put(w, ',');
        newline(w);
        f->nonempty = true;
    }
    return !w->failed;
}
void uslc_json_init(UslcJsonWriter *w, FILE *output, bool compact) {
    *w = (UslcJsonWriter){.output = output, .compact = compact};
}
static void begin(UslcJsonWriter *w, char kind) {
    if (!before(w))
        return;
    if (w->depth == sizeof w->frames / sizeof *w->frames) {
        w->failed = true;
        return;
    }
    put(w, kind);
    w->frames[w->depth++] = (UslcJsonFrame){.kind = kind};
}
void uslc_json_object(UslcJsonWriter *w) {
    begin(w, '{');
}
void uslc_json_array(UslcJsonWriter *w) {
    begin(w, '[');
}
void uslc_json_end(UslcJsonWriter *w) {
    if (w->failed)
        return;
    if (!w->depth || w->frames[w->depth - 1].needs_value) {
        w->failed = true;
        return;
    }
    UslcJsonFrame f = w->frames[--w->depth];
    if (f.nonempty)
        newline(w);
    put(w, f.kind == '{' ? '}' : ']');
}
void uslc_json_key(UslcJsonWriter *w, const char *key) {
    if (w->failed)
        return;
    if (!w->depth) {
        w->failed = true;
        return;
    }
    UslcJsonFrame *f = &w->frames[w->depth - 1];
    if (f->kind != '{' || f->needs_value) {
        w->failed = true;
        return;
    }
    if (f->nonempty)
        put(w, ',');
    newline(w);
    quoted(w, key, strlen(key));
    raw(w, w->compact ? ":" : ": ");
    f->nonempty = true;
    f->needs_value = true;
}
void uslc_json_slice(UslcJsonWriter *w, const char *text, size_t length) {
    if (before(w))
        quoted(w, text, length);
}
void uslc_json_string(UslcJsonWriter *w, const char *text) {
    uslc_json_slice(w, text, strlen(text));
}
void uslc_json_integer(UslcJsonWriter *w, int64_t value) {
    if (before(w) && fprintf(w->output, "%" PRId64, value) < 0)
        w->failed = true;
}
void uslc_json_size(UslcJsonWriter *w, size_t value) {
    if (value > INT64_MAX) {
        w->failed = true;
        return;
    }
    uslc_json_integer(w, (int64_t)value);
}
void uslc_json_boolean(UslcJsonWriter *w, bool value) {
    if (before(w))
        raw(w, value ? "true" : "false");
}
void uslc_json_null(UslcJsonWriter *w) {
    if (before(w))
        raw(w, "null");
}
bool uslc_json_finish(UslcJsonWriter *w) {
    if (w->depth || !w->root_written)
        w->failed = true;
    put(w, '\n');
    if (fflush(w->output) == EOF)
        w->failed = true;
    return !w->failed;
}
