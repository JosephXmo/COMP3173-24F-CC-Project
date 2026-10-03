#ifndef USLC_SERIALIZATION_H
#define USLC_SERIALIZATION_H
#include <stdio.h>
#include "uslc/parse_context.h"
struct json_object;
#ifdef USLC_TESTING
void uslc_json_test_fail_after(size_t successful_allocations);
#endif
/* Successful builders return one owned json-c reference; NULL is failure. */
struct json_object *uslc_json_tokens(UslcParseContext *context);
struct json_object *uslc_json_parse_tree(UslcParseContext *context);
struct json_object *uslc_json_ast(UslcParseContext *context, bool typed);
struct json_object *uslc_json_evaluation(UslcParseContext *context);
struct json_object *uslc_json_diagnostics(UslcParseContext *context);
bool uslc_json_write(UslcParseContext *context, FILE *file, struct json_object *object, bool compact);
bool uslc_artifacts_clear(UslcParseContext *context, const char *directory);
bool uslc_artifact_write(UslcParseContext *context, const char *directory,
                          const char *name, struct json_object *object, bool compact);
#endif
