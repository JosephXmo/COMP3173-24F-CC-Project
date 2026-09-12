#include "test.h"
USLC_TEST(uslc_test_evaluator) {
    struct { const char *source,*display; } cases[]={
        {"show 1+2*3.","7"},{"show 9-3-2.","4"},{"show (1+2)*3.","9"},
        {"show 0-9223372036854775807-1.","-9223372036854775808"},
        {"show 0*(0-9223372036854775807-1).","0"},
        {"show (0-3)*(0-4).","12"},{"show 3*(0-4).","-12"},
        {"show 1<2.","true"},{"show 1>2.","false"},{"show 1=1.","true"},{"show !1=2.","true"},
        {"show 1<2&2<3.","true"},{"show 1>2|2<3.","true"},
        {"show 1>2&9223372036854775807+1>0.","false"},
        {"show 1<2|9223372036854775807+1>0.","true"},
        {"show 1=1|0=1&0=1.","true"},
        {"let int x be 3. let int y be x*2. show y+1.","7"},
        {"let int lower be 3. let set positive be {x:x>lower}. show 5@positive.","true"},
        {"show 3@{x:x>3}.","false"},{"show 4@{x:x>3}.","true"},
        {"show 4@({x:x>3} I {y:y<5}).","true"},{"show 5@({x:x>3} I {y:y<5}).","false"},
        {"show 2@({x:x<3} U {y:y>5}).","true"},{"show 4@({x:x<3} U {y:y>5}).","false"},
        {"show {x:x>3} U {y:y>5}.","{ x : ((x > 3) | (x > 5)) }"},
        {"show {x:x>3} I {y:y<5}.","{ x : ((x > 3) & (x < 5)) }"},
        {"let int n be 3. show {x:x>n+2*4}.","{ x : (x > 11) }"},
        {"show {x:x=0|9223372036854775807+1>0}.","{ x : ((x = 0) | ((9223372036854775807 + 1) > 0)) }"},
        {"let int x be 99. show 2@{x:x<3}.","true"},
        {"show 4@{x:x@{x:x>3}}.","true"},
        {"show 4@{x:x@{y:y>x}}.","false"},
        {"show 4@{x:(x+1)@{y:y>x}}.","true"},
        {"show 4@({x:x@{y:y>x}} U {y:y=4}).","true"},
        {"show {x:x@{y:y>x}} U {y:y=4}.","{ x : ((x @ { y : (y > x) }) | (x = 4)) }"},
        {"let set a be {x:x>0}. let set b be a U {x:x<0}. show 0@b.","false"},
        {"show 0@{x:x=0|9223372036854775807+x>0}.","true"},
        {"show 1@({x:x=1} U {y:9223372036854775807+y>0}).","true"},
        {"show 1@({x:x=0} I {y:9223372036854775807+y>0}).","false"},
        {"show 5@{x:x@({y:y>x} U {z:z=x})}.","true"}
    };
    UslcParseContext c;
    for (size_t i=0;i<sizeof cases/sizeof *cases;++i) {
        bool ok=uslc_test_compile(&c,cases[i].source,4);USLC_EXPECT(ok);
        if (!ok) fprintf(stderr,"Source: %s (%s)\n",cases[i].source,c.diagnostics.message);
        if (c.result) { char *display=uslc_value_display(&c,c.result);USLC_EXPECT_STREQ(cases[i].display,display);free(display); }
        uslc_context_free(&c);
    }
    const char *overflow[]={"show 9223372036854775807+1.","show (0-9223372036854775807-1)-1.",
        "show 9223372036854775807*2.","show (0-9223372036854775807-1)*(0-1).",
        "show 1>0&9223372036854775807+1>0.","show 1<0|9223372036854775807+1>0.",
        "let int x be 9223372036854775807+1. show 0.","show 1@{x:9223372036854775807+x>0}."};
    for (size_t i=0;i<sizeof overflow/sizeof *overflow;++i) {
        USLC_EXPECT(!uslc_test_compile(&c,overflow[i],4));USLC_EXPECT_EQ_INT(4,c.diagnostics.status);USLC_EXPECT_STREQ("E4001",c.diagnostics.code);uslc_context_free(&c);
    }
    int64_t result=0;
    USLC_EXPECT(!uslc_checked_add(INT64_MAX,1,&result));USLC_EXPECT(!uslc_checked_add(INT64_MIN,-1,&result));
    USLC_EXPECT(!uslc_checked_sub(INT64_MIN,1,&result));USLC_EXPECT(!uslc_checked_sub(INT64_MAX,-1,&result));
    USLC_EXPECT(!uslc_checked_mul(INT64_MIN,-1,&result));USLC_EXPECT(!uslc_checked_mul(-1,INT64_MIN,&result));
    USLC_EXPECT(!uslc_checked_mul(INT64_MAX,2,&result));USLC_EXPECT(!uslc_checked_mul(INT64_MIN,2,&result));
    USLC_EXPECT(uslc_checked_mul(INT64_MIN,1,&result));USLC_EXPECT_EQ_INT(INT64_MIN,result);
    USLC_EXPECT(uslc_checked_mul(0,INT64_MIN,&result));USLC_EXPECT_EQ_INT(0,result);
    USLC_EXPECT(uslc_checked_add(INT64_MIN,INT64_MAX,&result));USLC_EXPECT_EQ_INT(-1,result);
}
