#include "uslc/printer.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "uslc/parse_context.h"
typedef struct { char *data;size_t length,capacity;UslcParseContext *context;bool failed; } UslcText;
typedef struct UslcPrintBinding { size_t binding,index;const struct UslcPrintBinding *parent; } UslcPrintBinding;
static void uslc_text_append(UslcText *t,const char *text) {
    size_t n=strlen(text);
    if (t->failed) return;
    void *data=t->data;
    if (n>=SIZE_MAX-t->length || !uslc_grow(&data,&t->capacity,t->length+n+1,1,&t->context->diagnostics)) {
        uslc_diagnose(&t->context->diagnostics,USLC_INTERNAL,"I9001",uslc_span_origin(),"display size overflow");t->failed=true;return;
    }
    t->data=data;
    memcpy(t->data+t->length,text,n+1);t->length+=n;
}
static void uslc_rep_name(char *buffer,size_t size,size_t index) {
    if (!index) snprintf(buffer,size,"x");
    else if (index==1) snprintf(buffer,size,"y");
    else if (index==2) snprintf(buffer,size,"z");
    else {
        /* Lowercase-only names stay within the source identifier alphabet. */
        char reverse[32];size_t n=0;
        do { reverse[n++]=(char)('a'+index%26);index/=26; } while(index && n<sizeof reverse);
        size_t out=0;if (out+1<size) buffer[out++]='x';
        while(n && out+1<size) buffer[out++]=reverse[--n];
        buffer[out]='\0';
    }
}
static void uslc_display_node(UslcText *t,const UslcValue *v,const UslcPrintBinding *scope,size_t depth) {
    if (t->failed || !v) return;
    if (depth>USLC_DEPTH_LIMIT) { uslc_diagnose(&t->context->diagnostics,USLC_INTERNAL,"I9004",v->span,"display depth limit exceeded");t->failed=true;return; }
    char buffer[64];
    switch(v->kind) {
    case USLC_VALUE_INT:snprintf(buffer,sizeof buffer,"%" PRId64,v->integer);uslc_text_append(t,buffer);break;
    case USLC_VALUE_BOOL:uslc_text_append(t,v->boolean ? "true" : "false");break;
    case USLC_VALUE_VAR: {
        const UslcPrintBinding *s=scope;
        while(s && s->binding!=v->binding) s=s->parent;
        if (!s) { uslc_diagnose(&t->context->diagnostics,USLC_INTERNAL,"E4002",v->span,"unbound representative in display");t->failed=true;return; }
        uslc_rep_name(buffer,sizeof buffer,s->index);uslc_text_append(t,buffer);break;
    }
    case USLC_VALUE_SET: {
        UslcPrintBinding local={v->binding,scope ? scope->index+1 : 0,scope};
        uslc_rep_name(buffer,sizeof buffer,local.index);
        uslc_text_append(t,"{ ");uslc_text_append(t,buffer);uslc_text_append(t," : ");
        uslc_display_node(t,v->left,&local,depth+1);uslc_text_append(t," }");break;
    }
    case USLC_VALUE_NOT:
        uslc_text_append(t,"!(");uslc_display_node(t,v->left,scope,depth+1);uslc_text_append(t,")");break;
    case USLC_VALUE_BINARY:
        uslc_text_append(t,"(");uslc_display_node(t,v->left,scope,depth+1);
        uslc_text_append(t," ");uslc_text_append(t,uslc_operator_name(v->op));uslc_text_append(t," ");
        uslc_display_node(t,v->right,scope,depth+1);uslc_text_append(t,")");break;
    }
}
char *uslc_value_display(UslcParseContext *c,const UslcValue *v) {
    UslcText text={.context=c};uslc_display_node(&text,v,NULL,1);
    if (text.failed) { free(text.data);return NULL; }
    return text.data;
}
static void uslc_print_excerpt(FILE *file,const UslcParseContext *c) {
    if (!c->source.data) return;
    size_t offset=c->diagnostics.span.start.offset;
    if (offset>c->source.length) offset=c->source.length;
    size_t start=offset,end=offset;
    while(start && c->source.data[start-1]!='\n' && c->source.data[start-1]!='\r') --start;
    while(end<c->source.length && c->source.data[end]!='\n' && c->source.data[end]!='\r') ++end;
    fputs("  excerpt:  ",file);
    for (size_t i=start;i<end;++i) {
        unsigned char ch=(unsigned char)c->source.data[i];
        if (ch>=32 && ch<127) fputc(ch,file);
        else fprintf(file,"\\x%02x",(unsigned)ch);
    }
    fputc('\n',file);
}
bool uslc_print_report(FILE *file,UslcParseContext *c) {
    char *display=NULL;
    if (!c->diagnostics.status && c->evaluated) {
        display=uslc_value_display(c,c->result);
        if (!display) return false;
    }
    fprintf(file,"UIC Set Language Compiler\nsource: %s\nstatus: %s\n\n",c->source.path ? c->source.path : "<unknown>",c->diagnostics.status ? "error" : "success");
    if (c->diagnostics.status) {
        UslcDiagnostics *d=&c->diagnostics;
        fprintf(file,"diagnostic:\n  phase:    %s\n  code:     %s\n  location: %zu:%zu-%zu:%zu\n  message:  %s\n",
            d->phase,d->code,d->span.start.line,d->span.start.column,d->span.end.line,d->span.end.column,d->message);
        uslc_print_excerpt(file,c);
        if (d->note[0]) fprintf(file,"  note:     %s\n",d->note);
    } else {
        fputs("phases:\n",file);
        if (c->lexed) fprintf(file,"  lexical:  ok (%zu tokens)\n",c->token_count);
        if (c->lowered) fprintf(file,"  syntax:   ok (%zu parse nodes)\n",c->parse_node_count+c->token_count-1);
        if (c->checked) fputs("  typing:   ok\n",file);
        if (c->evaluated) fprintf(file,"  evaluate: ok\n\nresult:\n  type:  %s\n  value: %s\n",uslc_type_name(c->result->type),display);
    }
    free(display);
    return !ferror(file) && fflush(file)==0;
}
