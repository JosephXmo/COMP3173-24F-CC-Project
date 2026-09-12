#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uslc/printer.h"
#include "uslc/type_checker.h"

typedef struct {
    int phase;
    const char *path, *emit_dir;
    bool compact, trace, grammar, slr;
} Options;
typedef struct {
    UslcSource source;
    UslcDiagnostics diagnostics;
    UslcTokens tokens;
    UslcGrammar grammar;
    UslcSlr slr;
    UslcArena parse_arena, ast_arena, value_arena;
    UslcParseNode *parse;
    UslcAst *ast;
    UslcSymbolTable symbols;
    const UslcValue *value;
    char *display;
    size_t node_count, bindings;
} Compilation;
static const char *const artifacts[] = {
    "tokens.json",     "parser_out.json",     "ast.json",
    "typing_out.json", "evaluation_out.json", "diagnostics.json"};
static void help(void) {
    (void)puts("uslc 1.0.0 - UIC Set Language Compiler\n"
               "usage: uslc [options] <source-file|->\n"
               "  --phase lex|parse|check|run  stop after a phase (default: run)\n"
               "  --emit-dir <path>           write JSON to an existing directory\n"
               "  --trace-parser              write shift/reduce trace to stderr\n"
               "  --dump-grammar              write productions and FIRST/FOLLOW to stderr\n"
               "  --dump-slr                  write LR(0) states and ACTION/GOTO to stderr\n"
               "  --compact-json              omit JSON indentation\n"
               "  --help                      show this help\n"
               "  --version                   show version\n"
               "  --                          end option parsing\n"
               "Dump options may also be used without a source file.");
}
static int options(int argc, char **argv, Options *o) {
    *o = (Options){.phase = 4};
    bool positional = false;
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!positional && !strcmp(a, "--")) {
            positional = true;
            continue;
        }
        if (!positional && !strcmp(a, "--help")) {
            help();
            return 1;
        }
        if (!positional && !strcmp(a, "--version")) {
            (void)puts("uslc 1.0.0 (core language)");
            return 1;
        }
        if (!positional && (!strcmp(a, "--phase") || !strcmp(a, "--emit-dir"))) {
            if (++i == argc)
                return -1;
            if (!strcmp(a, "--emit-dir")) {
                o->emit_dir = argv[i];
                if (!*o->emit_dir)
                    return -1;
            } else {
                static const char *const phases[] = {"lex", "parse", "check", "run"};
                o->phase = 0;
                for (int p = 0; p < 4; ++p)
                    if (!strcmp(argv[i], phases[p]))
                        o->phase = p + 1;
                if (!o->phase)
                    return -1;
            }
        } else if (!positional && !strcmp(a, "--trace-parser"))
            o->trace = true;
        else if (!positional && !strcmp(a, "--dump-grammar"))
            o->grammar = true;
        else if (!positional && !strcmp(a, "--dump-slr"))
            o->slr = true;
        else if (!positional && !strcmp(a, "--compact-json"))
            o->compact = true;
        else {
            if ((!positional && a[0] == '-' && strcmp(a, "-")) || o->path)
                return -1;
            o->path = a;
        }
    }
    return o->path || o->grammar || o->slr ? 0 : -1;
}
static void output_error(UslcDiagnostics *d, const char *path) {
    uslc_diagnostics_init(d);
    uslc_diagnose(d, 74, "I9074", "output", (UslcSpan){0},
                  "cannot write output '%s' (artifact directory must already exist)", path);
}
static char *artifact_path(const Options *o, size_t kind, UslcDiagnostics *d) {
    size_t a = strlen(o->emit_dir), b = strlen(artifacts[kind]);
    if (a > SIZE_MAX - b - 2) {
        uslc_oom(d);
        return NULL;
    }
    char *p = malloc(a + b + 2);
    if (!p) {
        uslc_oom(d);
        return NULL;
    }
    memcpy(p, o->emit_dir, a);
    p[a] = '/';
    memcpy(p + a + 1, artifacts[kind], b + 1);
    return p;
}
static bool emit(const Options *o, Compilation *c, size_t kind) {
    if (!o->emit_dir)
        return true;
    char *path = artifact_path(o, kind, &c->diagnostics);
    if (!path)
        return false;
    FILE *file = fopen(path, "wb");
    if (!file) {
        output_error(&c->diagnostics, path);
        free(path);
        return false;
    }
    UslcJsonWriter writer;
    uslc_json_init(&writer, file, o->compact);
    switch (kind) {
    case 0:
        uslc_json_tokens(&writer, &c->source, &c->tokens);
        break;
    case 1:
        uslc_json_parse_tree(&writer, &c->source, &c->grammar, c->parse);
        break;
    case 2:
        uslc_json_ast(&writer, &c->source, c->ast, NULL);
        break;
    case 3:
        uslc_json_ast(&writer, &c->source, c->ast, &c->symbols);
        break;
    case 4:
        uslc_json_evaluation(&writer, c->value, c->display);
        break;
    case 5:
        uslc_json_diagnostics(&writer, &c->source, &c->diagnostics);
        break;
    default:
        writer.failed = true;
        break;
    }
    bool ok = uslc_json_finish(&writer);
    if (fclose(file) == EOF)
        ok = false;
    if (!ok) {
        (void)remove(path);
        if (c->diagnostics.exit_code != 70)
            output_error(&c->diagnostics, path);
    }
    free(path);
    return ok;
}
static bool prepare_artifacts(const Options *o, Compilation *c) {
    if (!o->emit_dir)
        return true;
    for (size_t i = 0; i < sizeof artifacts / sizeof *artifacts; ++i) {
        char *path = artifact_path(o, i, &c->diagnostics);
        if (!path)
            return false;
        if (remove(path) != 0 && errno != ENOENT) {
            output_error(&c->diagnostics, path);
            free(path);
            return false;
        }
        free(path);
    }
    return true;
}
int main(int argc, char **argv) {
    Options o;
    int parsed = options(argc, argv, &o);
    if (parsed == 1)
        return fflush(stdout) == EOF ? 74 : 0;
    if (parsed < 0) {
        (void)fputs("uslc: invalid arguments; use --help\n", stderr);
        return 64;
    }
    Compilation *c = calloc(1, sizeof *c);
    if (!c) {
        (void)fputs("uslc: I9001: out of memory\n", stderr);
        return 70;
    }
    uslc_arena_init(&c->parse_arena);
    uslc_arena_init(&c->ast_arena);
    uslc_arena_init(&c->value_arena);
    c->source.name = o.path ? o.path : "<none>";
    int completed = 0;
    bool artifacts_ready = false;
    if (o.path) {
        FILE *input = !strcmp(o.path, "-") ? stdin : fopen(o.path, "rb");
        if (!input)
            uslc_diagnose(&c->diagnostics, 66, "I9066", "input", (UslcSpan){0},
                          "cannot open source '%s'", o.path);
        else {
            (void)uslc_source_read(&c->source, input, o.path, &c->diagnostics);
            if (input != stdin && fclose(input) == EOF)
                uslc_diagnose(&c->diagnostics, 66, "I9066", "input", (UslcSpan){0},
                              "cannot close source");
        }
        artifacts_ready = prepare_artifacts(&o, c);
    }
    if (!c->diagnostics.exit_code && (o.phase > 1 || o.grammar || o.slr)) {
        uslc_grammar_core(&c->grammar);
        if (uslc_grammar_analyze(&c->grammar, &c->diagnostics)) {
            if (o.grammar && !uslc_print_grammar(stderr, &c->grammar))
                output_error(&c->diagnostics, "stderr");
            if ((o.phase > 1 || o.slr) && !c->diagnostics.exit_code &&
                uslc_slr_build(&c->slr, &c->grammar, &c->diagnostics))
                if (o.slr && !uslc_print_slr(stderr, &c->slr))
                    output_error(&c->diagnostics, "stderr");
        }
    }
    if (o.path && !c->diagnostics.exit_code && uslc_lex(&c->tokens, &c->source, &c->diagnostics)) {
        completed = 1;
        if (emit(&o, c, 0) && o.phase > 1 &&
            uslc_parse(&c->slr, &c->tokens, &c->parse_arena, &c->parse, &c->node_count,
                       &c->diagnostics, o.trace ? uslc_print_trace : NULL, stderr)) {
            completed = 2;
            if (emit(&o, c, 1) &&
                uslc_ast_lower(c->parse, &c->ast_arena, &c->ast, &c->diagnostics) &&
                emit(&o, c, 2) && o.phase > 2 &&
                uslc_type_check(c->ast, &c->source, &c->symbols, &c->bindings, &c->diagnostics)) {
                completed = 3;
                if (emit(&o, c, 3) && o.phase > 3 &&
                    uslc_evaluate(c->ast, c->bindings, &c->value_arena, &c->value,
                                  &c->diagnostics)) {
                    /* Validate the complete symbolic display before reporting evaluation success.
                     */
                    c->display = uslc_value_display(c->value, &c->diagnostics);
                    if (c->display) {
                        completed = 4;
                        (void)emit(&o, c, 4);
                    }
                }
            }
        }
    }
    if (fflush(stderr) == EOF)
        output_error(&c->diagnostics, "stderr");
    if (artifacts_ready && c->diagnostics.exit_code != 74)
        (void)emit(&o, c, 5);
    if (o.path || c->diagnostics.exit_code) {
        FILE *output = c->diagnostics.exit_code ? stderr : stdout;
        if (!uslc_print_report(output, &c->source, &c->diagnostics, completed, c->tokens.count,
                               c->node_count, c->value, c->display) ||
            fflush(output) == EOF)
            output_error(&c->diagnostics, "report stream");
    }
    int status = c->diagnostics.exit_code;
    uslc_symbols_free(&c->symbols);
    uslc_tokens_free(&c->tokens);
    uslc_slr_free(&c->slr);
    uslc_arena_free(&c->parse_arena);
    uslc_arena_free(&c->ast_arena);
    uslc_arena_free(&c->value_arena);
    uslc_source_free(&c->source);
    free(c->display);
    free(c);
    return status;
}
