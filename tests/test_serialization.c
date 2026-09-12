#include "test.h"
#include <limits.h>
static void uslc_round_trip(struct json_object *o) {
    USLC_EXPECT(o!=NULL);if (!o) return;
    const char *text=json_object_to_json_string_ext(o,JSON_C_TO_STRING_PRETTY);
    USLC_EXPECT(text!=NULL);if (!text) return;
    struct json_tokener *tokener=json_tokener_new_ex(2048);USLC_EXPECT(tokener!=NULL);if (!tokener) return;
    struct json_object *copy=json_tokener_parse_ex(tokener,text,(int)strlen(text)+1);
    USLC_EXPECT_EQ_INT(json_tokener_success,json_tokener_get_error(tokener));USLC_EXPECT(copy && json_object_equal(o,copy));
    json_object_put(copy);json_tokener_free(tokener);
}
USLC_TEST(uslc_test_serialization) {
    UslcParseContext c;USLC_EXPECT(uslc_test_compile(&c,"show {x:x>3} U {y:y>5}.",4));
    c.source.path="quotes\" backslash\\ controls\t\n utf8-\xe9\x9b\x86.set";
    struct json_object *objects[]={uslc_json_tokens(&c),uslc_json_parse_tree(&c),uslc_json_ast(&c,false),uslc_json_ast(&c,true),uslc_json_evaluation(&c),uslc_json_diagnostics(&c)};
    for (size_t i=0;i<sizeof objects/sizeof *objects;++i) { uslc_round_trip(objects[i]);json_object_put(objects[i]); }
    struct json_object *o=uslc_json_tokens(&c),*source=NULL;
    USLC_EXPECT(json_object_object_get_ex(o,"source",&source));USLC_EXPECT_STREQ(c.source.path,json_object_get_string(source));
    const char *compact=json_object_to_json_string_ext(o,JSON_C_TO_STRING_PLAIN);
    const char *prefix="{\"schema\":\"uslc.tokens.v1\",\"source\":";
    USLC_EXPECT(strncmp(compact,prefix,strlen(prefix))==0);
    USLC_EXPECT(strstr(compact,"\\\"")!=NULL);USLC_EXPECT(strstr(compact,"\\\\")!=NULL);
    USLC_EXPECT(strstr(compact,"\\t")!=NULL);USLC_EXPECT(strstr(compact,"\\n")!=NULL);
    for (int mode=0;mode<2;++mode) {
        FILE *file=fopen("build/test-json.tmp","w+b");USLC_EXPECT(file!=NULL);
        if (file) {
            USLC_EXPECT(uslc_json_write(&c,file,o,mode!=0));rewind(file);
            UslcSource bytes={0};USLC_EXPECT(uslc_source_read(&bytes,file,"output",&c.diagnostics));
            USLC_EXPECT(bytes.length>0 && bytes.data[bytes.length-1]=='\n');
            if (!mode) USLC_EXPECT(strstr(bytes.data,"\n  \"schema\"")!=NULL);
            else USLC_EXPECT(strchr(bytes.data,'\n')==bytes.data+bytes.length-1);
            uslc_source_free(&bytes);fclose(file);
        }
    }
    json_object_put(o);uslc_context_free(&c);
    const char *scalars[]={"show 9223372036854775807.","show 0-9223372036854775807-1.","show 1<2."};
    for (size_t i=0;i<3;++i) {
        USLC_EXPECT(uslc_test_compile(&c,scalars[i],4));o=uslc_json_evaluation(&c);uslc_round_trip(o);
        struct json_object *result=json_object_object_get(o,"result"),*value=json_object_object_get(result,"value");
        if (i<2) { USLC_EXPECT(json_object_is_type(value,json_type_int));USLC_EXPECT_EQ_INT(i ? INT64_MIN : INT64_MAX,json_object_get_int64(value)); }
        else USLC_EXPECT(json_object_is_type(value,json_type_boolean));
        json_object_put(o);uslc_context_free(&c);
    }
    USLC_EXPECT(!uslc_test_compile(&c,"show \xff.",1));o=uslc_json_diagnostics(&c);uslc_round_trip(o);json_object_put(o);uslc_context_free(&c);
    USLC_EXPECT(uslc_test_compile(&c,"show 1.",4));o=uslc_json_evaluation(&c);
    FILE *full=fopen("/dev/full","wb");
    if (full) { USLC_EXPECT(!uslc_json_write(&c,full,o,false));USLC_EXPECT_EQ_INT(74,c.diagnostics.status);fclose(full); }
    json_object_put(o);uslc_context_free(&c);
    uslc_context_init(&c);USLC_EXPECT(!uslc_artifacts_clear(&c,"build/no-such-artifact-directory"));USLC_EXPECT_EQ_INT(74,c.diagnostics.status);uslc_context_free(&c);
#ifdef USLC_TESTING
    USLC_EXPECT(uslc_test_compile(&c,"show {x:x>3} U {y:y>5}.",4));
    bool succeeded=false;
    for (size_t budget=0;budget<300;++budget) {
        uslc_diagnostics_init(&c.diagnostics);uslc_json_test_fail_after(budget);
        o=uslc_json_evaluation(&c);
        if (o) { json_object_put(o);succeeded=true;break; }
        USLC_EXPECT_EQ_INT(70,c.diagnostics.status);USLC_EXPECT_STREQ("I9001",c.diagnostics.code);
    }
    USLC_EXPECT(succeeded);uslc_json_test_fail_after(SIZE_MAX);uslc_context_free(&c);
#endif
}
