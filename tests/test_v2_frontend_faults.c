/* Route owned symbol enumeration through the existing phase fault harness. */
#include <stdlib.h>
void *analysis_malloc(size_t, const char *);
#define malloc(size) analysis_malloc(size, __func__)
#include "../v2/frontend.c"
#undef malloc
