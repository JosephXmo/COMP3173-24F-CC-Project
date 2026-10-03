#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <json-c/json.h>
#include "uslc/evaluator.h"
#include "uslc/parse_context.h"
#include "uslc/printer.h"
#include "uslc/serialization.h"
#include "uslc/type_checker.h"
static void uslc_help(void) {
    puts("Usage: uslc [options] <source-file|->\n"
         "  --phase lex|parse|check|run  Stop after a phase (default: run)\n"
         "  --emit-dir <path>           Write artifacts to an existing directory\n"
         "  --trace-parser              Write Bison shift/reduce trace to stderr\n"
         "  --compact-json              Write compact artifact JSON\n"
         "  --help                      Show this help\n"
         "  --version                   Show language/compiler version\n"
         "  --                          End options");
}
static bool uslc_emit(UslcParseContext *c,const char *directory,const char *name,struct json_object *o,bool compact) {
    bool ok=uslc_artifact_write(c,directory,name,o,compact);json_object_put(o);return ok;
}
int main(int argc,char **argv) {
    const char *path=NULL,*directory=NULL;int phase=4;bool compact=false,trace=false,options=true;
    UslcParseContext context;uslc_context_init(&context);UslcParseContext *c=&context;
    (void)signal(SIGPIPE,SIG_IGN);
    for (int i=1;i<argc;++i) {
        const char *arg=argv[i];
        if (options && !strcmp(arg,"--")) { options=false;continue; }
        if (options && !strcmp(arg,"--help")) { uslc_help();uslc_context_free(c);return fflush(stdout)==0 ? 0 : 74; }
        if (options && !strcmp(arg,"--version")) { puts("uslc 1.0.0 (core language v1)");uslc_context_free(c);return fflush(stdout)==0 ? 0 : 74; }
        if (options && !strcmp(arg,"--trace-parser")) { trace=true;continue; }
        if (options && !strcmp(arg,"--compact-json")) { compact=true;continue; }
        if (options && (!strcmp(arg,"--phase") || !strcmp(arg,"--emit-dir"))) {
            if (++i==argc) goto usage;
            if (!strcmp(arg,"--emit-dir")) directory=argv[i];
            else if (!strcmp(argv[i],"lex")) phase=1;
            else if (!strcmp(argv[i],"parse")) phase=2;
            else if (!strcmp(argv[i],"check")) phase=3;
            else if (!strcmp(argv[i],"run")) phase=4;
            else goto usage;
            continue;
        }
        if ((options && arg[0]=='-' && arg[1]) || path) goto usage;
        path=arg;
    }
    if (!path) goto usage;
    c->source.path=path;
    if (directory && !uslc_artifacts_clear(c,directory)) goto report;
    FILE *input=!strcmp(path,"-") ? stdin : fopen(path,"rb");
    if (!input) { uslc_diagnose(&c->diagnostics,USLC_INPUT,"F6001",uslc_span_origin(),"cannot open input file");goto diagnostics; }
    bool read=uslc_source_read(&c->source,input,path,&c->diagnostics);
    if (input!=stdin && fclose(input)!=0) { uslc_diagnose(&c->diagnostics,USLC_INPUT,"F6001",uslc_span_origin(),"cannot close input file");read=false; }
    if (!read || !uslc_scan(c)) goto diagnostics;
    if (directory && !uslc_emit(c,directory,"tokens.json",uslc_json_tokens(c),compact)) goto report;
    if (phase==1) goto diagnostics;
    if (!uslc_parse_source(c,trace)) goto diagnostics;
    if (directory && !uslc_emit(c,directory,"parser_out.json",uslc_json_parse_tree(c),compact)) goto report;
    if (!uslc_lower(c)) goto diagnostics;
    if (directory && !uslc_emit(c,directory,"ast.json",uslc_json_ast(c,false),compact)) goto report;
    if (phase==2) goto diagnostics;
    if (!uslc_type_check(c)) goto diagnostics;
    if (directory && !uslc_emit(c,directory,"typing_out.json",uslc_json_ast(c,true),compact)) goto report;
    if (phase==3) goto diagnostics;
    if (!uslc_evaluate(c)) goto diagnostics;
    if (directory && !uslc_emit(c,directory,"evaluation_out.json",uslc_json_evaluation(c),compact)) goto report;
diagnostics:
    if (directory) (void)uslc_emit(c,directory,"diagnostics.json",uslc_json_diagnostics(c),compact);
report:
    if (!uslc_print_report(c->diagnostics.status ? stderr : stdout,c)) {
        if (ferror(stdout) || ferror(stderr)) {
            uslc_diagnostics_init(&c->diagnostics);
            uslc_diagnose(&c->diagnostics,USLC_OUTPUT,"F7401",uslc_span_origin(),"cannot write report");
        }
        (void)uslc_print_report(stderr,c);
    }
    int status=(int)c->diagnostics.status;uslc_context_free(c);return status;
usage:
    uslc_diagnose(&c->diagnostics,USLC_USAGE,"C6401",uslc_span_origin(),"invalid arguments; use --help for usage");goto report;
}
