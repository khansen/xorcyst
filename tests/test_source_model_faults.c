/* Test-only collector/guard mutations. No hooks or environment API ship in xasm. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static size_t allocation_index;
static int allocation_fault(void)
{
    const char *s = getenv("XASM_TEST_MODEL_ALLOC_AT");
    if (!s || allocation_index++ != (size_t)strtoul(s, NULL, 10)) return 0;
    fprintf(stderr, "INJECT_MODEL_ALLOC\n"); return 1;
}
static void *model_malloc(size_t n) { return allocation_fault() ? NULL : malloc(n); }
static void *model_calloc(size_t n, size_t size) { return allocation_fault() ? NULL : calloc(n, size); }
static void *model_realloc(void *p, size_t n) { return allocation_fault() ? NULL : realloc(p, n); }
static char *model_strdup(const char *s) { return allocation_fault() ? NULL : strdup(s); }
#define malloc model_malloc
#define calloc model_calloc
#define realloc model_realloc
#define strdup model_strdup
#define source_model_capture capture_real
#define source_model_prepare prepare_real
#define source_model_binding binding_real
#define source_model_write write_real
#include "../source_model.c"
#undef source_model_capture
#undef source_model_prepare
#undef source_model_binding
#undef source_model_write
#undef malloc
#undef calloc
#undef realloc
#undef strdup

int source_model_capture(astnode *root)
{
    const char *mutation = getenv("XASM_TEST_MODEL_MUTATION");
    if (mutation && !strcmp(mutation, "collector")) return 1;
    int result = capture_real(root);
    size_t i, keep = 0;
    if (!mutation) return result;
    for (i = 0; i < diagnostic_count; ++i) {
        int discard = (!strcmp(mutation, "capability") &&
                       !strcmp(diagnostics[i].code, "SOURCE_MODEL_UNSUPPORTED_CONSTRUCT")) ||
                      (!strcmp(mutation, "duplicate") &&
                       !strcmp(diagnostics[i].code, "SOURCE_MODEL_DUPLICATE_DECLARATION"));
        if (!discard) diagnostics[keep++] = diagnostics[i];
    }
    diagnostic_count = keep; failed = keep != 0;
    return !source_model_failed();
}

int source_model_write(const char *path, int certified, const char *comparison, const char *audit, int status)
{
    const char *mutation = getenv("XASM_TEST_MODEL_MUTATION");
    struct timespec start, end, cpu_start, cpu_end;
    int result;
    if (mutation && !strcmp(mutation, "access")) {
        size_t i;
        for (i = 0; i < node_count; ++i)
            if (nodes[i].source->type == INSTRUCTION_NODE && nodes[i].opcode == 0x85)
                nodes[i].opcode = 0xa5; /* write -> read, without changing native bytes */
    }
    if (mutation && !strcmp(mutation, "input-bytes")) {
        struct stat info;
        struct timespec times[2];
        FILE *f;
        if (stat(xasm_args.input_file, &info)) return 0;
        f = fopen(xasm_args.input_file, "r+b");
        if (!f) return 0;
        fputc('X', f); fclose(f);
#ifdef __APPLE__
        times[0] = info.st_atimespec; times[1] = info.st_mtimespec;
#else
        times[0] = info.st_atim; times[1] = info.st_mtim;
#endif
        if (utimensat(AT_FDCWD, xasm_args.input_file, times, 0)) return 0;
        fprintf(stderr, "INJECT_INPUT_SAME_SIZE_MTIME\n");
    }
    if (mutation && !strcmp(mutation, "input-delete")) unlink(xasm_args.input_file);
    if (mutation && !strcmp(mutation, "input-symlink")) {
        FILE *f = fopen("other.asm", "wb");
        if (!f) return 0;
        fputs("CLC\n", f); fclose(f);
        unlink(xasm_args.input_file);
        if (symlink("other.asm", xasm_args.input_file)) return 0;
    }
    if (mutation && !strcmp(mutation, "input-missing")) {
        FILE *f = fopen("late.inc", "wb");
        if (!f) return 0;
        fputs("NOP\n", f); fclose(f);
    }
    clock_gettime(CLOCK_MONOTONIC, &start);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);
    result = write_real(path, certified, comparison, audit, status);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);
    clock_gettime(CLOCK_MONOTONIC, &end);
    if (getenv("XASM_TEST_MODEL_MEASURE"))
        fprintf(stderr, "SOURCE_MODEL_MEASURE %.9f %.9f\n",
                end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) / 1e9,
                cpu_end.tv_sec - cpu_start.tv_sec + (cpu_end.tv_nsec - cpu_start.tv_nsec) / 1e9);
    return result;
}
void source_model_prepare(astnode *n, int pc)
{
    const char *mutation = getenv("XASM_TEST_MODEL_MUTATION");
    if (mutation && !strcmp(mutation, "unused") && n->type == EQU_NODE) return;
    prepare_real(n, pc);
}
void source_model_binding(size_t i, const location *loc)
{
    const char *mutation = getenv("XASM_TEST_MODEL_MUTATION");
    binding_real(i, loc);
    if (mutation && !strcmp(mutation, "binding") && i < node_count && nodes[i].reference) {
        size_t j;
        for (j = 0; j < node_count; ++j)
            if (nodes[j].is_declaration && !strcmp(declaration_name(j), "Other"))
                nodes[i].declaration = (unsigned)j + 1;
    }
}
