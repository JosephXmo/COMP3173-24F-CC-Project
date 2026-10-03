#ifndef USLC_STRING_H
#define USLC_STRING_H
#include <stddef.h>
/* Non-owning slice into the compilation's immutable source buffer. */
typedef struct { const char *data; size_t length; } UslcString;
#endif
