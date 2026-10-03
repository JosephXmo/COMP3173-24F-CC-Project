#include "uslc/parse_context.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#define YYSTYPE USLC_STYPE
#define YYLTYPE USLC_LTYPE
#include "lexer.h"
typedef struct UslcScanAllocation {
    struct UslcScanAllocation *next, *previous;
    size_t size;
    max_align_t data[];
} UslcScanAllocation;
#ifdef USLC_TESTING
static size_t uslc_scanner_allocation_budget=SIZE_MAX;
void uslc_scanner_test_fail_after(size_t successful_allocations) { uslc_scanner_allocation_budget=successful_allocations; }
#endif
/* Track even allocations not yet attached to a Flex buffer, so a fatal
   allocation failure can unwind through the wrapper without leaking. */
void *uslc_alloc(size_t size,yyscan_t scanner) {
#ifdef USLC_TESTING
    if (!uslc_scanner_allocation_budget) return NULL;
    if (uslc_scanner_allocation_budget!=SIZE_MAX) --uslc_scanner_allocation_budget;
#endif
    UslcScanner *s=uslc_get_extra(scanner);
    if (size>SIZE_MAX-sizeof(UslcScanAllocation)) return NULL;
    UslcScanAllocation *a=malloc(sizeof *a+size);
    if (!a) return NULL;
    a->size=size;a->previous=NULL;a->next=s->allocations;
    if (a->next) a->next->previous=a;
    s->allocations=a;return a->data;
}
void uslc_free(void *pointer,yyscan_t scanner) {
    if (!pointer) return;
    UslcScanner *s=uslc_get_extra(scanner);
    UslcScanAllocation *a=(UslcScanAllocation *)((unsigned char *)pointer - offsetof(UslcScanAllocation,data));
    if (a->previous) a->previous->next=a->next;else s->allocations=a->next;
    if (a->next) a->next->previous=a->previous;
    free(a);
}
void *uslc_realloc(void *pointer,size_t size,yyscan_t scanner) {
    if (!pointer) return uslc_alloc(size,scanner);
    UslcScanAllocation *a=(UslcScanAllocation *)((unsigned char *)pointer - offsetof(UslcScanAllocation,data));
    void *replacement=uslc_alloc(size,scanner);
    if (!replacement) return NULL;
    memcpy(replacement,pointer,a->size<size ? a->size : size);
    uslc_free(pointer,scanner);return replacement;
}
static_assert(alignof(UslcParseNode)<=alignof(max_align_t));
void uslc_context_init(UslcParseContext *c) {
    memset(c, 0, sizeof *c);
    uslc_diagnostics_init(&c->diagnostics);
    uslc_arena_init(&c->parse_arena, &c->diagnostics);
    uslc_arena_init(&c->ast_arena, &c->diagnostics);
    uslc_arena_init(&c->value_arena, &c->diagnostics);
    uslc_symbols_init(&c->symbols); c->next_binding=1;
}
void uslc_context_free(UslcParseContext *c) {
    uslc_symbols_free(&c->symbols); free(c->tokens);
    uslc_arena_free(&c->value_arena); uslc_arena_free(&c->ast_arena);
    uslc_arena_free(&c->parse_arena); uslc_source_free(&c->source);
    uslc_diagnostics_free(&c->diagnostics);
    memset(c, 0, sizeof *c);
}
void uslc_scanner_fatal(UslcScanner *s, const char *message) {
    uslc_diagnose(&s->context->diagnostics, USLC_INTERNAL, "I9001", uslc_span_origin(), message);
    longjmp(s->failure, 1);
}
bool uslc_scanner_init(UslcScanner *s, UslcParseContext *c, bool replay) {
    *s=(UslcScanner){.context=c, .position={1,1,0}, .replay=replay};
    if (c->source.length>(size_t)INT_MAX-2) {
        uslc_diagnose(&c->diagnostics, USLC_INTERNAL, "I9001", uslc_span_origin(), "source exceeds Flex buffer size limit"); return false;
    }
    if (uslc_lex_init_extra(s, &s->handle)) {
        uslc_diagnose(&c->diagnostics, USLC_INTERNAL, "I9001", uslc_span_origin(), "cannot initialize scanner"); return false;
    }
    if (setjmp(s->failure)) { uslc_scanner_destroy(s); return false; }
    s->buffer=uslc__scan_bytes(c->source.data, (int)c->source.length, s->handle);
    return true;
}
void uslc_scanner_destroy(UslcScanner *s) {
    if (s->handle) uslc_lex_destroy(s->handle);
    while (s->allocations) { UslcScanAllocation *next=s->allocations->next;free(s->allocations);s->allocations=next; }
    s->handle=NULL; s->buffer=NULL;
}
int uslc_scanner_next(UslcScanner *s, UslcParseNode **node) {
    *node=NULL;
    if (setjmp(s->failure)) return USLC_UNDEF;
    USLC_STYPE value={0}; USLC_LTYPE location=uslc_span_origin();
    int result=uslc_lex(&value, &location, s->handle);
    *node=value.node; return result;
}
void uslc_scanner_location(UslcScanner *s, UslcSpan *span, const char *text, size_t length) {
    span->start=s->position;
    for (size_t i=0; i<length; ++i) {
        unsigned char ch=(unsigned char)text[i]; ++s->position.offset;
        if (ch=='\r') { ++s->position.line; s->position.column=1; }
        else if (ch=='\n') { if (!s->previous_cr) ++s->position.line; s->position.column=1; }
        else ++s->position.column;
        s->previous_cr=ch=='\r';
    }
    span->end=s->position;
}
int uslc_scanner_token(UslcScanner *s, UslcParseNode **node, UslcSpan span, int kind, const char *symbol) {
    UslcParseContext *c=s->context;
    if (s->replay) {
        if (s->index<c->token_count) *node=c->tokens[s->index++];
        return kind;
    }
    if (s->eof) { *node=c->tokens[c->token_count-1]; return 0; }
    int64_t number=0;
    if (kind==NUM) {
        for (size_t i=span.start.offset; i<span.end.offset; ++i) {
            int digit=c->source.data[i]-'0';
            if (number>(INT64_MAX-digit)/10) {
                uslc_diagnose(&c->diagnostics, USLC_LEXICAL, "L1003", span, "integer literal out of range"); return USLC_UNDEF;
            }
            number=number*10+digit;
        }
    }
    UslcParseNode *n=uslc_arena_alloc(&c->parse_arena, sizeof *n);
    void *tokens=c->tokens;
    if (!n || !uslc_grow(&tokens, &c->token_capacity, c->token_count+1, sizeof *c->tokens, &c->diagnostics)) return USLC_UNDEF;
    c->tokens=tokens;
    *n=(UslcParseNode){.symbol=symbol, .production_id=-1, .token_kind=kind,
        .span=span, .integer=number, .height=1};
    c->tokens[c->token_count++]=n; *node=n;
    if (!kind) s->eof=true;
    return kind;
}
bool uslc_scan(UslcParseContext *c) {
    UslcScanner scanner;
    if (!uslc_scanner_init(&scanner,c,false)) return false;
    UslcParseNode *node;
    while (uslc_scanner_next(&scanner,&node) && !c->diagnostics.status) {}
    uslc_scanner_destroy(&scanner);
    c->lexed=c->diagnostics.status==USLC_OK; return c->lexed;
}
bool uslc_parse_source(UslcParseContext *c, bool trace) {
    if (!c->lexed) return false;
    size_t nesting=0;
    for (size_t i=0;i<c->token_count;++i) {
        int kind=c->tokens[i]->token_kind;
        if (kind==LPAREN || kind==LBRACE) {
            if (++nesting>USLC_DEPTH_LIMIT) {
                uslc_diagnose(&c->diagnostics,USLC_INTERNAL,"I9004",c->tokens[i]->span,"nesting limit exceeded (255)");return false;
            }
        } else if ((kind==RPAREN || kind==RBRACE) && nesting) --nesting;
    }
    UslcScanner *s=calloc(1,sizeof *s);
    if (!s) { uslc_diagnose(&c->diagnostics, USLC_INTERNAL,"I9001",uslc_span_origin(),"out of memory"); return false; }
    if (!uslc_scanner_init(s,c,true)) { free(s); return false; }
    uslc_debug=trace ? 1 : 0;
    int result=1;
    if (!setjmp(s->failure)) result=uslc_parse(c,s->handle);
    uslc_debug=0;
    uslc_scanner_destroy(s); free(s);
    c->parsed=result==0 && !c->diagnostics.status;
    return c->parsed;
}
UslcParseNode *uslc_reduce(UslcParseContext *c, const char *symbol, int production,
                          UslcSpan span, size_t count, ...) {
    UslcParseNode *n=uslc_arena_alloc(&c->parse_arena,sizeof *n);
    if (!n) return NULL;
    *n=(UslcParseNode){.symbol=symbol,.production_id=production,.span=span,.child_count=count,.height=1};
    if (count>6) { uslc_diagnose(&c->diagnostics,USLC_INTERNAL,"I9003",span,"malformed parse tree"); return NULL; }
    va_list args; va_start(args,count);
    for (size_t i=0;i<count;++i) {
        n->children[i]=va_arg(args,UslcParseNode *);
        if (n->children[i] && n->children[i]->height>=n->height) n->height=n->children[i]->height+1;
    }
    va_end(args);
    if (n->height>USLC_DEPTH_LIMIT) { uslc_diagnose(&c->diagnostics,USLC_INTERNAL,"I9004",span,"tree-depth limit exceeded (255)"); return NULL; }
    ++c->parse_node_count;
    return n;
}
const char *uslc_operator_name(int op) {
    switch (op) {
    case PLUS:return "+"; case MINUS:return "-"; case STAR:return "*";
    case UNION:return "U"; case INTERSECTION:return "I";
    case LT:return "<"; case GT:return ">"; case EQ:return "="; case MEMBER:return "@";
    case AND:return "&"; case OR:return "|"; case NOT:return "!";
    default:return "?";
    }
}
