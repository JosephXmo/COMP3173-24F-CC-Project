#ifndef USLC_TOKEN_H
#define USLC_TOKEN_H
#include <stdint.h>
#include "uslc/source.h"
typedef enum {
    USLC_DOT,
    USLC_LET,
    USLC_ID,
    USLC_BE,
    USLC_INT,
    USLC_SET,
    USLC_UNION,
    USLC_PLUS,
    USLC_MINUS,
    USLC_INTERSECTION,
    USLC_STAR,
    USLC_NUM,
    USLC_LPAREN,
    USLC_RPAREN,
    USLC_LBRACE,
    USLC_RBRACE,
    USLC_COLON,
    USLC_OR,
    USLC_AND,
    USLC_NOT,
    USLC_LT,
    USLC_GT,
    USLC_EQ,
    USLC_MEMBER,
    USLC_SHOW,
    USLC_EOF,
    USLC_ERROR
} UslcTokenKind;
typedef struct {
    UslcTokenKind kind;
    UslcSpan span;
    int64_t number;
} UslcToken;
const char *uslc_token_name(UslcTokenKind kind);
const char *uslc_token_spelling(UslcTokenKind kind);
bool uslc_token_equals(const UslcSource *s, UslcToken a, UslcToken b);
#endif
