#include "test.h"
USLC_TEST(json_escaping_and_pretty) {
    FILE *f = tmpfile();
    USLC_EXPECT(f != NULL);
    UslcJsonWriter w;
    uslc_json_init(&w, f, false);
    uslc_json_object(&w);
    uslc_json_key(&w, "text");
    uslc_json_slice(&w, "\"\\\b\f\n\r\t\0\x01\xc3\xa9", 11);
    uslc_json_key(&w, "values");
    uslc_json_array(&w);
    uslc_json_integer(&w, INT64_MIN);
    uslc_json_boolean(&w, true);
    uslc_json_null(&w);
    uslc_json_object(&w);
    uslc_json_end(&w);
    uslc_json_end(&w);
    uslc_json_end(&w);
    USLC_EXPECT(uslc_json_finish(&w));
    rewind(f);
    char buffer[512] = {0};
    USLC_EXPECT(fread(buffer, 1, sizeof buffer - 1, f) > 0 && !ferror(f));
    fclose(f);
    USLC_EXPECT_STREQ(
        "{\n  \"text\": \"\\\"\\\\\\b\\f\\n\\r\\t\\u0000\\u0001\xc3\xa9\",\n  \"values\": [\n    "
        "-9223372036854775808,\n    true,\n    null,\n    {}\n  ]\n}\n",
        buffer);
}
USLC_TEST(json_compact_and_state_errors) {
    FILE *f = tmpfile();
    USLC_EXPECT(f != NULL);
    UslcJsonWriter w;
    uslc_json_init(&w, f, true);
    uslc_json_array(&w);
    uslc_json_string(&w, "a");
    uslc_json_string(&w, "b");
    uslc_json_end(&w);
    USLC_EXPECT(uslc_json_finish(&w));
    rewind(f);
    char buffer[32] = {0};
    USLC_EXPECT(fread(buffer, 1, sizeof buffer - 1, f) > 0 && !ferror(f));
    USLC_EXPECT_STREQ("[\"a\",\"b\"]\n", buffer);
    uslc_json_init(&w, f, false);
    uslc_json_object(&w);
    uslc_json_key(&w, "missing");
    uslc_json_end(&w);
    USLC_EXPECT(!uslc_json_finish(&w));
    uslc_json_init(&w, f, false);
    uslc_json_integer(&w, 1);
    uslc_json_integer(&w, 2);
    USLC_EXPECT(!uslc_json_finish(&w));
    fclose(f);
}
USLC_TEST(json_write_failure) {
    FILE *f = fopen("tests/fixtures/read-only.txt", "rb");
    USLC_EXPECT(f != NULL);
    UslcJsonWriter w;
    uslc_json_init(&w, f, false);
    uslc_json_string(&w, "cannot write");
    USLC_EXPECT(!uslc_json_finish(&w));
    fclose(f);
}
void uslc_test_json_writer(void) {
    USLC_RUN(json_escaping_and_pretty);
    USLC_RUN(json_compact_and_state_errors);
    USLC_RUN(json_write_failure);
}
