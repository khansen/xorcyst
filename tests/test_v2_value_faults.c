/* Evaluate real value graphs with faults only during analysis serialization. */
#include <stdlib.h>
void *analysis_malloc(size_t, const char *);
void *analysis_calloc(size_t, size_t, const char *);
void *analysis_realloc(void *, size_t, const char *);
#define malloc(n) analysis_malloc(n, "value")
#define calloc(n, w) analysis_calloc(n, w, "value")
#define realloc(p, n) analysis_realloc(p, n, "value")
#include "../v2/value.c"
