#include "test.h"
#include <stdint.h>
int uslc_test_failures=0;
size_t uslc_test_assertions=0;
bool uslc_test_compile(UslcParseContext *c,const char *source,int phase) {
    uslc_context_init(c);
    if (!uslc_source_copy(&c->source,source,strlen(source),"test.set",&c->diagnostics) || !uslc_scan(c)) return false;
    if (phase==1) return true;
    if (!uslc_parse_source(c,false) || !uslc_lower(c)) return false;
    if (phase==2) return true;
    if (!uslc_type_check(c)) return false;
    return phase==3 || uslc_evaluate(c);
}
static void uslc_test_foundation(void) {
    UslcDiagnostics d;uslc_diagnostics_init(&d);UslcArena arena;uslc_arena_init(&arena,&d);
    for (size_t i=0;i<1000;++i) {
        void *p=uslc_arena_alloc(&arena,i%71+1);
        USLC_EXPECT(p!=NULL);USLC_EXPECT((uintptr_t)p%alignof(max_align_t)==0);
    }
    USLC_EXPECT(uslc_arena_alloc(&arena,SIZE_MAX)==NULL);USLC_EXPECT_STREQ("I9001",d.code);
    uslc_arena_free(&arena);uslc_arena_free(&arena);
    uslc_diagnostics_init(&d);void *array=NULL;size_t capacity=0;
    USLC_EXPECT(!uslc_grow(&array,&capacity,SIZE_MAX,2,&d));USLC_EXPECT(array==NULL);
    uslc_diagnostics_init(&d);
    UslcSource source={0};FILE *f=fopen("build/test-source.tmp","w+b");USLC_EXPECT(f!=NULL);
    if (f) {
        const char bytes[]="show\r\n3.\0tail";
        USLC_EXPECT(fwrite(bytes,1,sizeof bytes-1,f)==sizeof bytes-1);rewind(f);
        USLC_EXPECT(uslc_source_read(&source,f,"binary.set",&d));USLC_EXPECT_EQ_INT(sizeof bytes-1,source.length);
        USLC_EXPECT(memcmp(source.data,bytes,sizeof bytes-1)==0);fclose(f);uslc_source_free(&source);
    }
}
static int uslc_compare_json_files(const char *left,const char *right) {
    struct json_object *a=json_object_from_file(left),*b=json_object_from_file(right);
    bool equal=a && b && json_object_equal(a,b);json_object_put(a);json_object_put(b);
    if (!equal) fprintf(stderr,"JSON files differ: %s / %s\n",left,right);
    return equal ? 0 : 1;
}
int main(int argc,char **argv) {
    if (argc==4 && !strcmp(argv[1],"--compare-json")) return uslc_compare_json_files(argv[2],argv[3]);
    uslc_test_foundation();uslc_test_scanner();uslc_test_parser();uslc_test_type_checker();uslc_test_evaluator();uslc_test_serialization();
    printf("%zu assertions, %d failures\n",uslc_test_assertions,uslc_test_failures);
    return uslc_test_failures ? 1 : 0;
}
