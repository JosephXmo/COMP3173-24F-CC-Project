#include "uslc/serialization.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <json-c/json.h>
#include "uslc/evaluator.h"
#include "uslc/printer.h"
typedef struct { UslcParseContext *context;bool failed; } UslcJson;
#ifdef USLC_TESTING
static size_t uslc_json_allocation_budget=SIZE_MAX;
void uslc_json_test_fail_after(size_t successful_allocations) { uslc_json_allocation_budget=successful_allocations; }
#endif
static void uslc_json_fail(UslcJson *j) {
    j->failed=true;uslc_diagnose(&j->context->diagnostics,USLC_INTERNAL,"I9001",uslc_span_origin(),"JSON allocation failed");
}
static struct json_object *uslc_j_own(UslcJson *j,struct json_object *o) {
#ifdef USLC_TESTING
    if (!uslc_json_allocation_budget) { json_object_put(o);o=NULL; }
    else if (uslc_json_allocation_budget!=SIZE_MAX) --uslc_json_allocation_budget;
#endif
    if (!o) uslc_json_fail(j);return o;
}
static struct json_object *uslc_j_object(UslcJson *j) { return uslc_j_own(j,json_object_new_object()); }
static struct json_object *uslc_j_array(UslcJson *j) { return uslc_j_own(j,json_object_new_array()); }
static struct json_object *uslc_j_string(UslcJson *j,const char *s) { return uslc_j_own(j,json_object_new_string(s)); }
static struct json_object *uslc_j_bytes(UslcJson *j,const char *s,size_t n) {
    if (n>(size_t)INT_MAX) { uslc_json_fail(j);return NULL; }
    return uslc_j_own(j,json_object_new_string_len(s,(int)n));
}
static struct json_object *uslc_j_int(UslcJson *j,int64_t n) { return uslc_j_own(j,json_object_new_int64(n)); }
static struct json_object *uslc_j_size(UslcJson *j,size_t n) {
    if (n>(uint64_t)INT64_MAX) { uslc_json_fail(j);return NULL; }
    return uslc_j_int(j,(int64_t)n);
}
static void uslc_j_add(UslcJson *j,struct json_object *o,const char *key,struct json_object *value) {
    if (!o || json_object_object_add(o,key,value)<0) { json_object_put(value);uslc_json_fail(j); }
}
static void uslc_j_push(UslcJson *j,struct json_object *a,struct json_object *value) {
    if (!a || json_object_array_add(a,value)<0) { json_object_put(value);uslc_json_fail(j); }
}
static struct json_object *uslc_j_position(UslcJson *j,UslcPosition p) {
    struct json_object *o=uslc_j_object(j);
    uslc_j_add(j,o,"line",uslc_j_size(j,p.line));uslc_j_add(j,o,"column",uslc_j_size(j,p.column));uslc_j_add(j,o,"offset",uslc_j_size(j,p.offset));return o;
}
static struct json_object *uslc_j_span(UslcJson *j,UslcSpan span) {
    struct json_object *o=uslc_j_object(j);
    uslc_j_add(j,o,"start",uslc_j_position(j,span.start));uslc_j_add(j,o,"end",uslc_j_position(j,span.end));return o;
}
static struct json_object *uslc_j_lexeme(UslcJson *j,const UslcParseNode *token) {
    return uslc_j_bytes(j,j->context->source.data+token->span.start.offset,token->span.end.offset-token->span.start.offset);
}
static struct json_object *uslc_j_token(UslcJson *j,const UslcParseNode *token) {
    struct json_object *o=uslc_j_object(j);
    uslc_j_add(j,o,"kind",uslc_j_string(j,token->symbol));uslc_j_add(j,o,"lexeme",uslc_j_lexeme(j,token));
    uslc_j_add(j,o,"span",uslc_j_span(j,token->span));
    if (!strcmp(token->symbol,"NUM")) uslc_j_add(j,o,"value",uslc_j_int(j,token->integer));return o;
}
static struct json_object *uslc_j_document(UslcJson *j,const char *schema,bool source) {
    struct json_object *o=uslc_j_object(j);uslc_j_add(j,o,"schema",uslc_j_string(j,schema));
    if (source) uslc_j_add(j,o,"source",uslc_j_string(j,j->context->source.path ? j->context->source.path : "<unknown>"));return o;
}
static struct json_object *uslc_j_finish(UslcJson *j,struct json_object *o) {
    if (j->failed) { json_object_put(o);return NULL; }return o;
}
struct json_object *uslc_json_tokens(UslcParseContext *c) {
    UslcJson j={.context=c};struct json_object *o=uslc_j_document(&j,"uslc.tokens.v1",true),*a=uslc_j_array(&j);
    for (size_t i=0;i<c->token_count && !j.failed;++i) uslc_j_push(&j,a,uslc_j_token(&j,c->tokens[i]));
    uslc_j_add(&j,o,"tokens",a);return uslc_j_finish(&j,o);
}
static struct json_object *uslc_j_parse_node(UslcJson *j,const UslcParseNode *n) {
    if (j->failed || !n) { uslc_json_fail(j);return NULL; }
    struct json_object *o=uslc_j_object(j),*a=uslc_j_array(j);
    uslc_j_add(j,o,"symbol",uslc_j_string(j,n->symbol));
    uslc_j_add(j,o,"production_id",n->production_id<0 ? NULL : uslc_j_int(j,n->production_id));
    uslc_j_add(j,o,"span",uslc_j_span(j,n->span));
    for (size_t i=0;i<n->child_count && !j->failed;++i) uslc_j_push(j,a,uslc_j_parse_node(j,n->children[i]));
    uslc_j_add(j,o,"children",a);
    if (n->production_id<0) uslc_j_add(j,o,"token",uslc_j_token(j,n));return o;
}
struct json_object *uslc_json_parse_tree(UslcParseContext *c) {
    UslcJson j={.context=c};struct json_object *o=uslc_j_document(&j,"uslc.parser.v1",true);
    uslc_j_add(&j,o,"tree",uslc_j_parse_node(&j,c->root));return uslc_j_finish(&j,o);
}
static struct json_object *uslc_j_ast_node(UslcJson *j,const UslcAst *n,bool typed) {
    if (j->failed || !n) { uslc_json_fail(j);return NULL; }
    struct json_object *o=uslc_j_object(j);
    uslc_j_add(j,o,"kind",uslc_j_string(j,uslc_ast_kind_name(n->kind)));uslc_j_add(j,o,"span",uslc_j_span(j,n->span));
    uslc_j_add(j,o,"type",typed ? uslc_j_string(j,uslc_type_name(n->type)) : NULL);
    if (n->kind==USLC_NAME || n->kind==USLC_DECLARATION || n->kind==USLC_SET_LITERAL) {
        uslc_j_add(j,o,"name",uslc_j_lexeme(j,n->token));
        if (typed) uslc_j_add(j,o,"binding_id",uslc_j_size(j,n->binding));
    }
    if (n->kind==USLC_DECLARATION) uslc_j_add(j,o,"declared_type",uslc_j_string(j,uslc_type_name(n->declared_type)));
    if (n->kind==USLC_INT_LITERAL) uslc_j_add(j,o,"value",uslc_j_int(j,n->integer));
    if (n->op && n->kind!=USLC_SHOW) uslc_j_add(j,o,"operator",uslc_j_string(j,uslc_operator_name(n->op)));
    if (n->kind==USLC_PROGRAM) {
        struct json_object *a=uslc_j_array(j);
        for (const UslcAst *d=n->left;d && !j->failed;d=d->next) uslc_j_push(j,a,uslc_j_ast_node(j,d,typed));
        uslc_j_add(j,o,"declarations",a);uslc_j_add(j,o,"command",uslc_j_ast_node(j,n->right,typed));
    } else {
        if (n->left) uslc_j_add(j,o,"left",uslc_j_ast_node(j,n->left,typed));
        if (n->right) uslc_j_add(j,o,"right",uslc_j_ast_node(j,n->right,typed));
    }
    return o;
}
struct json_object *uslc_json_ast(UslcParseContext *c,bool typed) {
    UslcJson j={.context=c};struct json_object *o=uslc_j_document(&j,typed ? "uslc.typing.v1" : "uslc.ast.v1",true);
    uslc_j_add(&j,o,"tree",uslc_j_ast_node(&j,c->ast,typed));return uslc_j_finish(&j,o);
}
static struct json_object *uslc_j_value(UslcJson *j,const UslcValue *v) {
    if (!v || j->failed) { uslc_json_fail(j);return NULL; }
    static const char *const kinds[]={"IntegerLiteral","BooleanLiteral","Representative","SetValue","Binary","LogicalNot"};
    struct json_object *o=uslc_j_object(j);
    uslc_j_add(j,o,"kind",uslc_j_string(j,kinds[v->kind]));uslc_j_add(j,o,"type",uslc_j_string(j,uslc_type_name(v->type)));
    if (v->kind==USLC_VALUE_INT) uslc_j_add(j,o,"value",uslc_j_int(j,v->integer));
    if (v->kind==USLC_VALUE_BOOL) uslc_j_add(j,o,"value",uslc_j_own(j,json_object_new_boolean(v->boolean)));
    if (v->kind==USLC_VALUE_VAR || v->kind==USLC_VALUE_SET) uslc_j_add(j,o,"binding_id",uslc_j_size(j,v->binding));
    if (v->kind==USLC_VALUE_BINARY || v->kind==USLC_VALUE_NOT) uslc_j_add(j,o,"operator",uslc_j_string(j,uslc_operator_name(v->op)));
    if (v->left) uslc_j_add(j,o,v->kind==USLC_VALUE_SET ? "predicate" : "left",uslc_j_value(j,v->left));
    if (v->right) uslc_j_add(j,o,"right",uslc_j_value(j,v->right));return o;
}
struct json_object *uslc_json_evaluation(UslcParseContext *c) {
    UslcJson j={.context=c};struct json_object *o=uslc_j_document(&j,"uslc.evaluation.v1",false),*r=uslc_j_object(&j);
    uslc_j_add(&j,o,"status",uslc_j_string(&j,"success"));
    if (!c->result) { json_object_put(r);json_object_put(o);uslc_json_fail(&j);return NULL; }
    const UslcValue *v=c->result;uslc_j_add(&j,r,"type",uslc_j_string(&j,uslc_type_name(v->type)));
    if (v->type==USLC_INTEGER) uslc_j_add(&j,r,"value",uslc_j_int(&j,v->integer));
    else if (v->type==USLC_BOOLEAN) uslc_j_add(&j,r,"value",uslc_j_own(&j,json_object_new_boolean(v->boolean)));
    else {
        uslc_j_add(&j,r,"value",uslc_j_value(&j,v));char *display=uslc_value_display(c,v);
        if (!display) uslc_json_fail(&j);
        else { uslc_j_add(&j,r,"display",uslc_j_string(&j,display));free(display); }
    }
    uslc_j_add(&j,o,"result",r);return uslc_j_finish(&j,o);
}
struct json_object *uslc_json_diagnostics(UslcParseContext *c) {
    UslcJson j={.context=c};struct json_object *o=uslc_j_document(&j,"uslc.diagnostics.v1",true),*a=uslc_j_array(&j);
    UslcDiagnostics *d=&c->diagnostics;
    if (d->status) {
        struct json_object *item=uslc_j_object(&j);
        uslc_j_add(&j,item,"phase",uslc_j_string(&j,d->phase));uslc_j_add(&j,item,"code",uslc_j_string(&j,d->code));
        uslc_j_add(&j,item,"span",uslc_j_span(&j,d->span));uslc_j_add(&j,item,"message",uslc_j_string(&j,d->message));
        if (c->source.data) {
            size_t start=d->span.start.offset,end=start;
            if (start>c->source.length) start=end=c->source.length;
            while(start && c->source.data[start-1]!='\n' && c->source.data[start-1]!='\r') --start;
            while(end<c->source.length && c->source.data[end]!='\n' && c->source.data[end]!='\r') ++end;
            /* Escaping invalid input bytes here is an excerpt presentation choice;
               JSON quoting itself is exclusively handled by json-c. */
            if (end-start>(SIZE_MAX-1)/4) uslc_json_fail(&j);
            else {
                char *excerpt=malloc((end-start)*4+1);
                if (!excerpt) uslc_json_fail(&j);
                else {
                    size_t length=0;
                    for (size_t i=start;i<end;++i) {
                        unsigned char ch=(unsigned char)c->source.data[i];
                        if (ch>=32 && ch<127) excerpt[length++]=(char)ch;
                        else { snprintf(excerpt+length,5,"\\x%02x",(unsigned)ch);length+=4; }
                    }
                    excerpt[length]='\0';uslc_j_add(&j,item,"excerpt",uslc_j_bytes(&j,excerpt,length));free(excerpt);
                }
            }
        }
        if (d->note[0]) { struct json_object *notes=uslc_j_array(&j);uslc_j_push(&j,notes,uslc_j_string(&j,d->note));uslc_j_add(&j,item,"notes",notes); }
        uslc_j_push(&j,a,item);
    }
    uslc_j_add(&j,o,"diagnostics",a);return uslc_j_finish(&j,o);
}
static void uslc_output_error(UslcParseContext *c,const char *message) {
    uslc_diagnostics_init(&c->diagnostics);
    uslc_diagnose(&c->diagnostics,USLC_OUTPUT,"F7401",uslc_span_origin(),message);
}
bool uslc_json_write(UslcParseContext *c,FILE *file,struct json_object *o,bool compact) {
    if (!o) return false;
    const char *text=json_object_to_json_string_ext(o,compact ? JSON_C_TO_STRING_PLAIN : JSON_C_TO_STRING_PRETTY);
    if (!text) { uslc_diagnose(&c->diagnostics,USLC_INTERNAL,"I9001",uslc_span_origin(),"JSON serialization failed");return false; }
    if (fputs(text,file)==EOF || fputc('\n',file)==EOF || fflush(file)!=0) { uslc_output_error(c,"cannot write JSON output");return false; }
    return true;
}
static char *uslc_artifact_path(UslcParseContext *c,const char *directory,const char *name) {
    size_t a=strlen(directory),b=strlen(name);
    if (a>SIZE_MAX-b-2) { uslc_output_error(c,"output path too long");return NULL; }
    char *path=malloc(a+b+2);
    if (!path) { uslc_diagnose(&c->diagnostics,USLC_INTERNAL,"I9001",uslc_span_origin(),"out of memory");return NULL; }
    memcpy(path,directory,a);path[a]='/';memcpy(path+a+1,name,b+1);return path;
}
bool uslc_artifacts_clear(UslcParseContext *c,const char *directory) {
    static const char *const names[]={"tokens.json","parser_out.json","ast.json","typing_out.json","evaluation_out.json","diagnostics.json"};
    struct stat st;
    if (stat(directory,&st)!=0 || !S_ISDIR(st.st_mode)) { uslc_output_error(c,"artifact directory must already exist");return false; }
    for (size_t i=0;i<sizeof names/sizeof *names;++i) {
        char *path=uslc_artifact_path(c,directory,names[i]);if (!path) return false;
        if (lstat(path,&st)==0 && S_ISDIR(st.st_mode)) {
            free(path);uslc_output_error(c,"an artifact path is a directory");return false;
        }
        /* Never recursively remove directories or follow a stale symlink. */
        int result=remove(path);int error=errno;free(path);
        if (result!=0 && error!=ENOENT) { uslc_output_error(c,"cannot clear stale artifact");return false; }
    }
    return true;
}
bool uslc_artifact_write(UslcParseContext *c,const char *directory,const char *name,struct json_object *o,bool compact) {
    if (!o) return false;
    char *path=uslc_artifact_path(c,directory,name);if (!path) return false;
    FILE *file=fopen(path,"wb");
    if (!file) { free(path);uslc_output_error(c,"cannot open artifact for writing");return false; }
    bool ok=uslc_json_write(c,file,o,compact);
    if (fclose(file)!=0) { uslc_output_error(c,"cannot close artifact");ok=false; }
    if (!ok) (void)remove(path);
    free(path);return ok;
}
