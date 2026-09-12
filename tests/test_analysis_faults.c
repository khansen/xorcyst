/* Phase-scoped faults exercise real collection/projection code, with no
   allocator hooks or environment switches in the production executable. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../listing.h"

static int allocations, fail_at, active, address_only;
static const char *phase;

static void begin_fault_phase(const char *name)
{
    const char *selected = getenv("XASM_TEST_ALLOC_PHASE");
    const char *index = getenv("XASM_TEST_ALLOC_AT");
    if (selected != NULL && strcmp(selected, "constants") == 0 && strcmp(name, "collect") == 0)
        name = "constants";
    if (selected != NULL && strcmp(selected, "xref") == 0 && strcmp(name, "address") == 0)
        name = "xref";
    phase = name;
    active = selected != NULL && strcmp(selected, name) == 0;
    allocations = 0;
    fail_at = index == NULL ? -1 : atoi(index);
    address_only = strcmp(name, "address") == 0;
}
static void end_fault_phase(void)
{
    if (active) fprintf(stderr, "ALLOCATIONS %s %d\n", phase, allocations);
    active = 0;
}
static int allocation_fails(const char *function)
{
    int index;
    if (!active || (address_only && strcmp(function, "build_xref_address_index") != 0)) return 0;
    if (strcmp(phase, "constants") == 0 && strcmp(function, "symtab_list_type") != 0) return 0;
    index = allocations++;
    if (fail_at < 0) fprintf(stderr, "ALLOC_SITE %s %d %s\n", phase, index, function);
    if (index != fail_at) return 0;
    fprintf(stderr, "INJECT_ALLOC %s %d %s\n", phase, index, function);
    return 1;
}
static void *analysis_malloc(size_t size, const char *function)
{
    return allocation_fails(function) ? NULL : malloc(size);
}
static void *analysis_calloc(size_t count, size_t size, const char *function)
{
    return allocation_fails(function) ? NULL : calloc(count, size);
}
static void *analysis_realloc(void *ptr, size_t size, const char *function)
{
    return allocation_fails(function) ? NULL : realloc(ptr, size);
}

static FILE *xref_stream;

static int xref_io_fails(const char *operation)
{
    const char *selected = getenv("XASM_TEST_IO_FAILURE");
    if (selected == NULL || strcmp(selected, operation) != 0) return 0;
    fprintf(stderr, "INJECT_IO %s\n", operation);
    return 1;
}

static int xref_setvbuf(FILE *fp, char *buffer, int mode, size_t size)
{
    xref_stream = fp;
    return xref_io_fails("xref_setvbuf") ? -1 : setvbuf(fp, buffer, mode, size);
}

static int xref_fclose(FILE *fp)
{
    int is_xref = fp == xref_stream;
    int result = fclose(fp);
    if (is_xref) xref_stream = NULL;
    return is_xref && xref_io_fails("xref_fclose") ? EOF : result;
}

#define setvbuf xref_setvbuf
#define fclose xref_fclose
#define malloc(size) analysis_malloc(size, __func__)
#define calloc(count, size) analysis_calloc(count, size, __func__)
#define realloc(ptr, size) analysis_realloc(ptr, size, __func__)
#define collect_analysis collect_analysis_impl
#define plan_analysis_outputs plan_analysis_outputs_impl
#define prepare_analysis_outputs prepare_analysis_outputs_impl
#define write_analysis_outputs write_analysis_outputs_impl
#include "../listing.c"
#undef setvbuf
#undef fclose
#undef malloc
#undef calloc
#undef realloc
#undef collect_analysis
#undef plan_analysis_outputs
#undef prepare_analysis_outputs
#undef write_analysis_outputs

/* Enumeration allocations belong to shared collection too. Earlier assembler
   passes still use the real allocator because no collection phase is active. */
#define malloc(size) analysis_malloc(size, __func__)
#undef SAFE_FREE
#include "../symtab.c"
#undef malloc

analysis_result *collect_analysis(astnode *root, const analysis_options *options)
{
    analysis_result *result;
    begin_fault_phase("collect");
    result = collect_analysis_impl(root, options);
    end_fault_phase();
    return result;
}
analysis_output_plan *plan_analysis_outputs(const analysis_result *analysis,
                                            const analysis_output_options *options)
{
    analysis_output_plan *result;
    begin_fault_phase("plan");
    result = plan_analysis_outputs_impl(analysis, options);
    end_fault_phase();
    return result;
}
int prepare_analysis_outputs(analysis_result *analysis, const analysis_output_plan *plan)
{
    int result;
    begin_fault_phase("prepare");
    result = prepare_analysis_outputs_impl(analysis, plan);
    end_fault_phase();
    return result;
}
int write_analysis_outputs(analysis_result *analysis, const analysis_output_plan *plan)
{
    int result;
    begin_fault_phase("address");
    result = write_analysis_outputs_impl(analysis, plan);
    end_fault_phase();
    return result;
}
