#include "test.h"
#include <stdint.h>
USLC_TEST(alignment_and_growth) {
    UslcArena a;
    UslcDiagnostics d = {0};
    uslc_arena_init(&a);
    for (size_t i = 0; i < 500; ++i) {
        unsigned char *p = uslc_arena_alloc(&a, i + 1, &d);
        USLC_EXPECT(p != NULL);
        USLC_EXPECT((uintptr_t)p % _Alignof(UslcMaxAlign) == 0);
        USLC_EXPECT((uintptr_t)p % _Alignof(UslcParseNode) == 0);
        USLC_EXPECT((uintptr_t)p % _Alignof(UslcAst) == 0);
        USLC_EXPECT((uintptr_t)p % _Alignof(UslcValue) == 0);
        for (size_t j = 0; j <= i; ++j)
            USLC_EXPECT(p[j] == 0);
    }
    uslc_arena_free(&a);
    uslc_arena_free(&a);
}
USLC_TEST(allocation_failure) {
    UslcArena a;
    UslcDiagnostics d = {0};
    uslc_arena_init(&a);
    a.fail_after = 0;
    USLC_EXPECT(uslc_arena_alloc(&a, 1, &d) == NULL);
    USLC_EXPECT_STREQ("I9001", d.code);
    uslc_arena_free(&a);
    uslc_diagnostics_init(&d);
    USLC_EXPECT(uslc_arena_alloc(&a, SIZE_MAX, &d) == NULL);
    void *data = NULL;
    size_t cap = 0;
    USLC_EXPECT(!uslc_reserve(&data, &cap, SIZE_MAX, 2, &d));
    free(data);
    uslc_arena_free(&a);
}
USLC_TEST(source_stream) {
    FILE *f = tmpfile();
    USLC_EXPECT(f != NULL);
    const char sample[] = "show 1 .\r\n\0tail";
    USLC_EXPECT(fwrite(sample, 1, sizeof sample - 1, f) == sizeof sample - 1);
    rewind(f);
    UslcSource s = {0};
    UslcDiagnostics d = {0};
    USLC_EXPECT(uslc_source_read(&s, f, "source", &d));
    USLC_EXPECT(s.length == sizeof sample - 1);
    USLC_EXPECT(!memcmp(s.data, sample, s.length));
    USLC_EXPECT(s.data[s.length] == 0);
    uslc_source_free(&s);
    fclose(f);
}
void uslc_test_arena(void) {
    USLC_RUN(alignment_and_growth);
    USLC_RUN(allocation_failure);
    USLC_RUN(source_stream);
}
