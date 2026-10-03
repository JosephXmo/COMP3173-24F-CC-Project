#include "uslc/printer.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    char *data;
    size_t length, capacity;
    UslcDiagnostics *d;
} Text;
static bool text(Text *t, const char *s) {
    size_t n = strlen(s);
    void *data = t->data;
    if (n > SIZE_MAX - t->length - 1 ||
        !uslc_reserve(&data, &t->capacity, t->length + n + 1, 1, t->d))
        return uslc_oom(t->d);
    t->data = data;
    memcpy(t->data + t->length, s, n + 1);
    t->length += n;
    return true;
}
typedef struct DisplayScope {
    size_t binding, depth;
    const struct DisplayScope *parent;
} DisplayScope;
static bool representative(Text *t, size_t depth) {
    char name[32] = "x";
    size_t n = 1;
    while (depth) {
        name[n++] = (char)('a' + (depth - 1) % 26);
        depth = (depth - 1) / 26;
    }
    name[n] = '\0';
    return text(t, name);
}
static bool display(Text *t, const UslcValue *v, const DisplayScope *scope, size_t depth) {
    if (!uslc_depth(t->d, depth, v->span))
        return false;
    if (v->kind == USLC_VALUE_INTEGER) {
        char buffer[32];
        (void)snprintf(buffer, sizeof buffer, "%" PRId64, v->integer);
        return text(t, buffer);
    }
    if (v->kind == USLC_VALUE_BOOLEAN)
        return text(t, v->boolean ? "true" : "false");
    if (v->kind == USLC_VALUE_VARIABLE) {
        for (const DisplayScope *s = scope; s; s = s->parent)
            if (s->binding == v->binding)
                return representative(t, s->depth);
        return uslc_diagnose(t->d, 70, "E4002", "internal", v->span,
                             "unbound representative in display");
    }
    if (v->kind == USLC_VALUE_SET) {
        DisplayScope local = {v->binding, scope ? scope->depth + 1 : 0, scope};
        return text(t, "{ ") && representative(t, local.depth) && text(t, " : ") &&
               display(t, v->left, &local, depth + 1) && text(t, " }");
    }
    if (v->kind == USLC_VALUE_NOT)
        return text(t, "!(") && display(t, v->left, scope, depth + 1) && text(t, ")");
    return text(t, "(") && display(t, v->left, scope, depth + 1) && text(t, " ") &&
           text(t, uslc_token_spelling(v->op)) && text(t, " ") &&
           display(t, v->right, scope, depth + 1) && text(t, ")");
}
char *uslc_value_display(const UslcValue *value, UslcDiagnostics *d) {
    Text t = {.d = d};
    if (!display(&t, value, NULL, 0)) {
        free(t.data);
        return NULL;
    }
    return t.data;
}
static void production(FILE *out, const UslcGrammar *g, size_t p, size_t dot) {
    const UslcProduction *r = &g->productions[p];
    (void)fprintf(out, "%zu. %s ->", p, g->names[r->lhs]);
    for (size_t i = 0; i <= r->length; ++i) {
        if (i == dot)
            (void)fputs(" .", out);
        if (i < r->length)
            (void)fprintf(out, " %s", g->names[r->rhs[i]]);
    }
    if (!r->length && dot == SIZE_MAX)
        (void)fputs(" epsilon", out);
    (void)fputc('\n', out);
}
static void symbol_set(FILE *out, const UslcGrammar *g, uint64_t bits) {
    (void)fputc('{', out);
    bool first = true;
    for (size_t i = 0; i < g->terminals; ++i)
        if (bits & (UINT64_C(1) << i)) {
            (void)fprintf(out, "%s%s", first ? "" : ", ", g->names[i]);
            first = false;
        }
    (void)fputc('}', out);
}
bool uslc_print_grammar(FILE *out, const UslcGrammar *g) {
    for (size_t p = 0; p < g->count; ++p)
        production(out, g, p, SIZE_MAX);
    for (size_t s = g->terminals; s < g->symbols; ++s) {
        (void)fprintf(out, "%s: nullable=%s FIRST=", g->names[s],
                      g->nullable[s] ? "true" : "false");
        symbol_set(out, g, g->first[s]);
        (void)fputs(" FOLLOW=", out);
        symbol_set(out, g, g->follow[s]);
        (void)fputc('\n', out);
    }
    return !ferror(out);
}
static const char *action_name(UslcActionKind kind) {
    static const char *const names[] = {"error", "shift", "reduce", "accept"};
    return names[kind];
}
bool uslc_print_slr(FILE *out, const UslcSlr *slr) {
    const UslcGrammar *g = slr->grammar;
    for (size_t s = 0; s < slr->count; ++s) {
        (void)fprintf(out, "state %zu:\n", s);
        for (size_t p = 0; p < g->count; ++p)
            for (size_t dot = 0; dot <= g->productions[p].length; ++dot)
                if (uslc_items_has(&slr->states[s].items, (UslcItem){p, dot})) {
                    (void)fputs("  ", out);
                    production(out, g, p, dot);
                }
        for (size_t t = 0; t < g->terminals; ++t) {
            UslcAction a = slr->states[s].actions[t];
            if (a.kind)
                (void)fprintf(out, "  ACTION[%s] = %s %zu\n", g->names[t], action_name(a.kind),
                              a.target);
        }
        for (size_t n = g->terminals; n < g->symbols; ++n)
            if (slr->states[s].transitions[n] != SIZE_MAX)
                (void)fprintf(out, "  GOTO[%s] = %zu\n", g->names[n],
                              slr->states[s].transitions[n]);
    }
    return !ferror(out);
}
void uslc_print_trace(void *context, size_t state, UslcToken token, UslcAction action) {
    FILE *out = context;
    (void)fprintf(out, "state %zu, %s at %zu:%zu: %s %zu\n", state, uslc_token_name(token.kind),
                  token.span.start.line, token.span.start.column, action_name(action.kind),
                  action.target);
}
static void excerpt(FILE *out, const UslcSource *s, UslcSpan span) {
    if (!s->data || !span.start.line || span.start.offset > s->length)
        return;
    size_t begin = span.start.offset, end = span.start.offset;
    while (begin && s->data[begin - 1] != '\n' && s->data[begin - 1] != '\r')
        --begin;
    while (end < s->length && s->data[end] != '\n' && s->data[end] != '\r')
        ++end;
    char label[32];
    int width = snprintf(label, sizeof label, "%zu", span.start.line);
    (void)fprintf(out, "\n%s | ", label);
    for (size_t i = begin; i < end; ++i) {
        unsigned char c = (unsigned char)s->data[i];
        (void)fputc(c >= 32 && c < 127 ? c : ' ', out);
    }
    (void)fputc('\n', out);
    for (int i = 0; i < width; ++i)
        (void)fputc(' ', out);
    (void)fputs(" | ", out);
    for (size_t i = begin; i < span.start.offset; ++i)
        (void)fputc(' ', out);
    (void)fputs("^\n", out);
}
bool uslc_print_report(FILE *out, const UslcSource *source, const UslcDiagnostics *d, int completed,
                       size_t token_count, size_t node_count, const UslcValue *value,
                       const char *value_display) {
    (void)fprintf(out, "UIC Set Language Compiler\nsource: %s\nstatus: %s\n",
                  source->name ? source->name : "<none>", d->exit_code ? "error" : "success");
    if (d->exit_code) {
        (void)fprintf(out,
                      "\ndiagnostic:\n  phase:    %s\n  code:     %s\n  location: "
                      "%zu:%zu-%zu:%zu\n  message:  %s\n",
                      d->phase, d->code, d->span.start.line, d->span.start.column, d->span.end.line,
                      d->span.end.column, d->message);
        excerpt(out, source, d->span);
    } else {
        (void)fputs("\nphases:\n", out);
        if (completed >= 1)
            (void)fprintf(out, "  lexical:  ok (%zu tokens)\n", token_count);
        if (completed >= 2)
            (void)fprintf(out, "  syntax:   ok (%zu parse nodes)\n", node_count);
        if (completed >= 3)
            (void)fputs("  typing:   ok\n", out);
        if (completed >= 4)
            (void)fputs("  evaluate: ok\n", out);
        if (value) {
            if (!value_display)
                return false;
            (void)fprintf(out, "\nresult:\n  type:  %s\n  value: %s\n", uslc_type_name(value->type),
                          value_display);
        }
    }
    return !ferror(out);
}
static void key_string(UslcJsonWriter *w, const char *key, const char *value) {
    uslc_json_key(w, key);
    uslc_json_string(w, value);
}
static void key_size(UslcJsonWriter *w, const char *key, size_t value) {
    uslc_json_key(w, key);
    uslc_json_size(w, value);
}
static void position(UslcJsonWriter *w, UslcPosition p) {
    uslc_json_object(w);
    key_size(w, "line", p.line);
    key_size(w, "column", p.column);
    key_size(w, "offset", p.offset);
    uslc_json_end(w);
}
static void span(UslcJsonWriter *w, UslcSpan s) {
    uslc_json_key(w, "span");
    uslc_json_object(w);
    uslc_json_key(w, "start");
    position(w, s.start);
    uslc_json_key(w, "end");
    position(w, s.end);
    uslc_json_end(w);
}
static void lexeme(UslcJsonWriter *w, const char *key, const UslcSource *s, UslcToken t) {
    uslc_json_key(w, key);
    uslc_json_slice(w, s->data + t.span.start.offset, t.span.end.offset - t.span.start.offset);
}
static void token(UslcJsonWriter *w, const UslcSource *s, UslcToken t) {
    uslc_json_object(w);
    key_string(w, "kind", uslc_token_name(t.kind));
    lexeme(w, "lexeme", s, t);
    span(w, t.span);
    if (t.kind == USLC_NUM) {
        uslc_json_key(w, "value");
        uslc_json_integer(w, t.number);
    }
    uslc_json_end(w);
}
static void document(UslcJsonWriter *w, const char *schema, const UslcSource *s) {
    uslc_json_object(w);
    key_string(w, "schema", schema);
    if (s)
        key_string(w, "source", s->name);
}
void uslc_json_tokens(UslcJsonWriter *w, const UslcSource *source, const UslcTokens *tokens) {
    document(w, "uslc.tokens.v1", source);
    uslc_json_key(w, "tokens");
    uslc_json_array(w);
    for (size_t i = 0; i < tokens->count; ++i)
        token(w, source, tokens->data[i]);
    uslc_json_end(w);
    uslc_json_end(w);
}
static void parse_node(UslcJsonWriter *w, const UslcSource *s, const UslcGrammar *g,
                       const UslcParseNode *n) {
    if (w->failed)
        return;
    uslc_json_object(w);
    key_string(w, "symbol", g->names[n->symbol]);
    uslc_json_key(w, "production_id");
    if (n->production == SIZE_MAX)
        uslc_json_null(w);
    else
        uslc_json_size(w, n->production);
    span(w, n->span);
    if (n->production == SIZE_MAX) {
        uslc_json_key(w, "token");
        token(w, s, n->token);
    }
    uslc_json_key(w, "children");
    uslc_json_array(w);
    for (size_t i = 0; i < n->count; ++i)
        parse_node(w, s, g, n->children[i]);
    uslc_json_end(w);
    uslc_json_end(w);
}
void uslc_json_parse_tree(UslcJsonWriter *w, const UslcSource *source, const UslcGrammar *g,
                          const UslcParseNode *root) {
    document(w, "uslc.parser.v1", source);
    uslc_json_key(w, "root");
    parse_node(w, source, g, root);
    uslc_json_end(w);
}
static void ast_node(UslcJsonWriter *w, const UslcSource *s, const UslcAst *a) {
    if (w->failed)
        return;
    uslc_json_object(w);
    key_string(w, "kind", uslc_ast_kind_name(a->kind));
    span(w, a->span);
    key_string(w, "type", uslc_type_name(a->type));
    if (a->binding != SIZE_MAX)
        key_size(w, "binding", a->binding);
    if (a->kind == USLC_AST_DECLARATION || a->kind == USLC_AST_NAME || a->kind == USLC_AST_SET)
        lexeme(w, "name", s, a->token);
    if (a->kind == USLC_AST_DECLARATION)
        key_string(w, "declared_type", uslc_type_name(a->declared_type));
    if (a->kind == USLC_AST_INTEGER) {
        uslc_json_key(w, "value");
        uslc_json_integer(w, a->token.number);
    }
    if (a->kind >= USLC_AST_ARITHMETIC)
        key_string(w, "operator", uslc_token_spelling(a->token.kind));
    uslc_json_key(w, "children");
    uslc_json_array(w);
    for (const UslcAst *child = a->left; child; child = child->next)
        ast_node(w, s, child);
    if (a->right)
        ast_node(w, s, a->right);
    uslc_json_end(w);
    uslc_json_end(w);
}
void uslc_json_ast(UslcJsonWriter *w, const UslcSource *source, const UslcAst *root,
                   const UslcSymbolTable *symbols) {
    document(w, symbols ? "uslc.typing.v1" : "uslc.ast.v1", source);
    if (symbols) {
        key_string(w, "status", "success");
        uslc_json_key(w, "symbols");
        uslc_json_array(w);
        for (size_t i = 0; i < symbols->count; ++i) {
            UslcSymbol s = symbols->entries[i];
            uslc_json_object(w);
            lexeme(w, "name", source, s.name);
            key_string(w, "type", uslc_type_name(s.type));
            key_size(w, "binding", s.binding);
            uslc_json_end(w);
        }
        uslc_json_end(w);
    }
    uslc_json_key(w, "root");
    ast_node(w, source, root);
    uslc_json_end(w);
}
static void symbolic(UslcJsonWriter *w, const UslcValue *v) {
    if (w->failed)
        return;
    static const char *const kinds[] = {"Integer", "Boolean", "Variable", "Set", "Binary", "Not"};
    uslc_json_object(w);
    key_string(w, "kind", kinds[v->kind]);
    key_string(w, "type", uslc_type_name(v->type));
    switch (v->kind) {
    case USLC_VALUE_INTEGER:
        uslc_json_key(w, "value");
        uslc_json_integer(w, v->integer);
        break;
    case USLC_VALUE_BOOLEAN:
        uslc_json_key(w, "value");
        uslc_json_boolean(w, v->boolean);
        break;
    case USLC_VALUE_VARIABLE:
        key_size(w, "binding", v->binding);
        break;
    case USLC_VALUE_SET:
        key_size(w, "binding", v->binding);
        uslc_json_key(w, "predicate");
        symbolic(w, v->left);
        break;
    default:
        key_string(w, "operator", uslc_token_spelling(v->op));
        uslc_json_key(w, "left");
        symbolic(w, v->left);
        if (v->right) {
            uslc_json_key(w, "right");
            symbolic(w, v->right);
        }
        break;
    }
    uslc_json_end(w);
}
void uslc_json_evaluation(UslcJsonWriter *w, const UslcValue *value, const char *value_display) {
    document(w, "uslc.evaluation.v1", NULL);
    key_string(w, "status", "success");
    uslc_json_key(w, "result");
    uslc_json_object(w);
    key_string(w, "type", uslc_type_name(value->type));
    uslc_json_key(w, "value");
    if (value->kind == USLC_VALUE_INTEGER)
        uslc_json_integer(w, value->integer);
    else if (value->kind == USLC_VALUE_BOOLEAN)
        uslc_json_boolean(w, value->boolean);
    else {
        symbolic(w, value);
        if (value_display)
            key_string(w, "display", value_display);
        else
            w->failed = true;
    }
    uslc_json_end(w);
    uslc_json_end(w);
}
void uslc_json_diagnostics(UslcJsonWriter *w, const UslcSource *source, const UslcDiagnostics *d) {
    document(w, "uslc.diagnostics.v1", source);
    uslc_json_key(w, "diagnostics");
    uslc_json_array(w);
    if (d->exit_code) {
        uslc_json_object(w);
        key_string(w, "code", d->code);
        key_string(w, "phase", d->phase);
        key_string(w, "message", d->message);
        span(w, d->span);
        uslc_json_end(w);
    }
    uslc_json_end(w);
    uslc_json_end(w);
}
