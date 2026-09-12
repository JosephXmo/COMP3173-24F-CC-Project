#include "uslc/lexer.h"
#include <stdlib.h>
#include <string.h>
#include "uslc/arena.h"

static bool lower(unsigned char c) {
    return c >= 'a' && c <= 'z';
}
static bool digit(unsigned char c) {
    return c >= '0' && c <= '9';
}
static unsigned char peek(const UslcLexer *l) {
    return (unsigned char)l->source->data[l->position.offset];
}
static void advance(UslcLexer *l) {
    unsigned char c = peek(l);
    ++l->position.offset;
    if (c == '\r') {
        ++l->position.line;
        l->position.column = 1;
    } else if (c == '\n') {
        if (!l->previous_cr)
            ++l->position.line;
        l->position.column = 1;
    } else
        ++l->position.column;
    l->previous_cr = c == '\r';
}
void uslc_lexer_init(UslcLexer *l, const UslcSource *s, UslcDiagnostics *d) {
    *l = (UslcLexer){.source = s, .diagnostics = d, .position = {1, 1, 0}};
}
UslcToken uslc_lexer_next(UslcLexer *l) {
    while (l->position.offset < l->source->length && strchr(" \t\r\n\f\v", peek(l)) && peek(l))
        advance(l);
    UslcToken t = {.kind = USLC_EOF, .span = {l->position, l->position}};
    if (l->position.offset == l->source->length)
        return t;
    unsigned char c = peek(l);
    const char *error = NULL, *message = NULL;
    if (lower(c)) {
        do {
            advance(l);
        } while (l->position.offset < l->source->length && lower(peek(l)));
        t.kind = USLC_ID;
        static const UslcTokenKind keywords[] = {USLC_LET, USLC_BE, USLC_INT, USLC_SET, USLC_SHOW};
        size_t n = l->position.offset - t.span.start.offset;
        for (size_t i = 0; i < sizeof keywords / sizeof *keywords; ++i) {
            const char *word = uslc_token_spelling(keywords[i]);
            if (n == strlen(word) && !memcmp(l->source->data + t.span.start.offset, word, n))
                t.kind = keywords[i];
        }
    } else if (digit(c)) {
        bool overflow = false;
        do {
            int64_t value = (int64_t)(peek(l) - '0');
            if (t.number > (INT64_MAX - value) / 10)
                overflow = true;
            if (!overflow)
                t.number = t.number * 10 + value;
            advance(l);
        } while (l->position.offset < l->source->length && digit(peek(l)));
        t.kind = USLC_NUM;
        if (c == '0' && l->position.offset - t.span.start.offset > 1) {
            error = "L1002";
            message = "leading zero in integer literal";
        } else if (overflow) {
            error = "L1003";
            message = "integer literal exceeds int64_t range";
        }
    } else {
        advance(l);
        t.kind = USLC_ERROR;
        for (int k = 0; k < USLC_EOF; ++k) {
            const char *word = uslc_token_spelling((UslcTokenKind)k);
            if (word[0] == (char)c && word[1] == '\0') {
                t.kind = (UslcTokenKind)k;
                break;
            }
        }
        if (t.kind == USLC_ERROR) {
            error = "L1001";
            message = "invalid character";
        }
    }
    t.span.end = l->position;
    if (error) {
        t.kind = USLC_ERROR;
        uslc_diagnose(l->diagnostics, 1, error, "lexical", t.span, "%s", message);
    }
    return t;
}
bool uslc_lex(UslcTokens *tokens, const UslcSource *s, UslcDiagnostics *d) {
    UslcLexer l;
    uslc_lexer_init(&l, s, d);
    do {
        UslcToken t = uslc_lexer_next(&l);
        if (d->exit_code)
            return false;
        void *data = tokens->data;
        if (tokens->count == SIZE_MAX ||
            !uslc_reserve(&data, &tokens->capacity, tokens->count + 1, sizeof *tokens->data, d))
            return false;
        tokens->data = data;
        tokens->data[tokens->count++] = t;
        if (t.kind == USLC_EOF)
            return true;
    } while (true);
}
void uslc_tokens_init(UslcTokens *tokens) {
    *tokens = (UslcTokens){0};
}
void uslc_tokens_free(UslcTokens *tokens) {
    free(tokens->data);
    uslc_tokens_init(tokens);
}
