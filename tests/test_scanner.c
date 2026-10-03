#include "test.h"
#include "parser.h"
USLC_TEST(uslc_test_scanner) {
    UslcParseContext c;
    const char *source="let be int set show letbe be123+a 0 9223372036854775807 + - * < > = @ & | ! U I . ( ) { } :";
    const int kinds[]={LET,BE,INT,SET,SHOW,ID,BE,NUM,PLUS,ID,NUM,NUM,PLUS,MINUS,STAR,LT,GT,EQ,MEMBER,AND,OR,NOT,UNION,INTERSECTION,DOT,LPAREN,RPAREN,LBRACE,RBRACE,COLON,0};
    USLC_EXPECT(uslc_test_compile(&c,source,1));USLC_EXPECT_EQ_INT(sizeof kinds/sizeof *kinds,c.token_count);
    for (size_t i=0;i<c.token_count && i<sizeof kinds/sizeof *kinds;++i) USLC_EXPECT_EQ_INT(kinds[i],c.tokens[i]->token_kind);
    USLC_EXPECT_EQ_INT(INT64_MAX,c.tokens[11]->integer);uslc_context_free(&c);
    USLC_EXPECT(uslc_test_compile(&c,"show\r\n\t12\r+\n3\f\v.",1));
    USLC_EXPECT_EQ_INT(2,c.tokens[1]->span.start.line);USLC_EXPECT_EQ_INT(2,c.tokens[1]->span.start.column);
    USLC_EXPECT_EQ_INT(7,c.tokens[1]->span.start.offset);USLC_EXPECT_EQ_INT(9,c.tokens[1]->span.end.offset);
    USLC_EXPECT_EQ_INT(3,c.tokens[2]->span.start.line);USLC_EXPECT_EQ_INT(4,c.tokens[3]->span.start.line);
    USLC_EXPECT_EQ_INT(c.source.length,c.tokens[c.token_count-1]->span.end.offset);uslc_context_free(&c);
    const char *invalid[]={"012","00","0001","9223372036854775808","9999999999999999999999999999","A","_","/","#","\xc3\xa9"};
    const char *codes[]={"L1002","L1002","L1002","L1003","L1003","L1001","L1001","L1001","L1001","L1001"};
    for (size_t i=0;i<sizeof invalid/sizeof *invalid;++i) {
        USLC_EXPECT(!uslc_test_compile(&c,invalid[i],1));USLC_EXPECT_EQ_INT(1,c.diagnostics.status);USLC_EXPECT_STREQ(codes[i],c.diagnostics.code);uslc_context_free(&c);
    }
    uslc_context_init(&c);const char nul[]={'s','h','o','w',' ',0,'.'};
    USLC_EXPECT(uslc_source_copy(&c.source,nul,sizeof nul,"nul.set",&c.diagnostics));USLC_EXPECT(!uslc_scan(&c));
    USLC_EXPECT_EQ_INT(5,c.diagnostics.span.start.offset);uslc_context_free(&c);
    for (int repeat=0;repeat<20;++repeat) {
        UslcParseContext a,b;uslc_context_init(&a);uslc_context_init(&b);
        USLC_EXPECT(uslc_source_copy(&a.source,"show\n1.",7,"a",&a.diagnostics));
        USLC_EXPECT(uslc_source_copy(&b.source,"let int x",9,"b",&b.diagnostics));
        UslcScanner sa,sb;USLC_EXPECT(uslc_scanner_init(&sa,&a,false));USLC_EXPECT(uslc_scanner_init(&sb,&b,false));
        UslcParseNode *na,*nb;
        USLC_EXPECT_EQ_INT(SHOW,uslc_scanner_next(&sa,&na));USLC_EXPECT_EQ_INT(LET,uslc_scanner_next(&sb,&nb));
        USLC_EXPECT_EQ_INT(NUM,uslc_scanner_next(&sa,&na));USLC_EXPECT_EQ_INT(INT,uslc_scanner_next(&sb,&nb));
        USLC_EXPECT_EQ_INT(2,na->span.start.line);USLC_EXPECT_EQ_INT(1,nb->span.start.line);
        uslc_scanner_destroy(&sa);uslc_scanner_destroy(&sb);uslc_context_free(&a);uslc_context_free(&b);
    }
#ifdef USLC_TESTING
    bool succeeded=false;
    for (size_t budget=0;budget<16;++budget) {
        uslc_scanner_test_fail_after(budget);
        bool ok=uslc_test_compile(&c,"show 3.",1);
        if (ok) succeeded=true;
        else { USLC_EXPECT_EQ_INT(70,c.diagnostics.status);USLC_EXPECT_STREQ("I9001",c.diagnostics.code); }
        uslc_context_free(&c);if (ok) break;
    }
    USLC_EXPECT(succeeded);uslc_scanner_test_fail_after(SIZE_MAX);
#endif
}
