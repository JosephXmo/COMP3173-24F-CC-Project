#ifndef USLC_TEST_H
#define USLC_TEST_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include "uslc/evaluator.h"
#include "uslc/parse_context.h"
#include "uslc/printer.h"
#include "uslc/serialization.h"
#include "uslc/type_checker.h"
extern int uslc_test_failures;
extern size_t uslc_test_assertions;
#define USLC_TEST(name) void name(void)
#define USLC_EXPECT(condition) do { ++uslc_test_assertions; if (!(condition)) { \
    fprintf(stderr,"%s: %s:%d: failed: %s\n",__func__,__FILE__,__LINE__,#condition);++uslc_test_failures; } } while(0)
#define USLC_EXPECT_EQ_INT(expected,actual) do { int64_t e_=(int64_t)(expected),a_=(int64_t)(actual); \
    ++uslc_test_assertions;if(e_!=a_) { fprintf(stderr,"%s: %s:%d: expected %lld, got %lld\n", \
    __func__,__FILE__,__LINE__,(long long)e_,(long long)a_);++uslc_test_failures; } } while(0)
#define USLC_EXPECT_STREQ(expected,actual) do { const char *e_=(expected),*a_=(actual);++uslc_test_assertions; \
    if(!e_ || !a_ || strcmp(e_,a_)) { fprintf(stderr,"%s: %s:%d: expected [%s], got [%s]\n", \
    __func__,__FILE__,__LINE__,e_ ? e_ : "NULL",a_ ? a_ : "NULL");++uslc_test_failures; } } while(0)
bool uslc_test_compile(UslcParseContext *c,const char *source,int phase);
void uslc_test_scanner(void);
void uslc_test_parser(void);
void uslc_test_type_checker(void);
void uslc_test_evaluator(void);
void uslc_test_serialization(void);
#endif
