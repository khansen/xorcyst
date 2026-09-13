/* Compile the real CLI/exporter with failures confined to output operations. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int io_fault(const char *operation)
{
    static int occurrence;
    const char *selected = getenv("XASM_TEST_IO_FAILURE");
    const char *index = getenv("XASM_TEST_IO_AT");
    if (selected == NULL || strcmp(selected, operation) != 0) return 0;
    if (occurrence++ != (index == NULL ? 0 : atoi(index))) return 0;
    fprintf(stderr, "INJECT_IO %s\n", operation);
    errno = EIO;
    return 1;
}

static FILE *binary_stream;

static int binary_fstat(int fd, struct stat *info)
{
    return io_fault("binary_fstat") ? -1 : fstat(fd, info);
}
static FILE *binary_fdopen(int fd, const char *mode)
{
    binary_stream = io_fault("binary_fdopen") ? NULL : fdopen(fd, mode);
    return binary_stream;
}
static int binary_ferror(FILE *fp)
{
    return fp == binary_stream && io_fault("binary_ferror") ? 1 : ferror(fp);
}
static int binary_fclose(FILE *fp)
{
    int is_binary = fp == binary_stream;
    int result;
    if (is_binary) binary_stream = NULL;
    result = fclose(fp);
    return is_binary && io_fault("binary_fclose") ? EOF : result;
}
static int binary_rename(const char *from, const char *to)
{
    return io_fault("binary_rename") ? -1 : rename(from, to);
}
#define fdopen binary_fdopen
#define fstat binary_fstat
#define ferror binary_ferror
#define fclose binary_fclose
#define rename binary_rename
#include "../xasm.c"
#undef fdopen
#undef fstat
#undef ferror
#undef fclose
#undef rename

/* The writer owns all sidecar streams. Select failures by output destination,
   keeping binary faults above independent of the sidecar publication order. */
#include "../output_file.h"
static const char *fault_output_path;

static int output_fault(const char *operation)
{
    const char *selected = getenv("XASM_TEST_OUTPUT_PATH");
    const char *path = fault_output_path != NULL ? fault_output_path : "stdout";
    if (selected != NULL && strcmp(selected, path) != 0) return 0;
    return io_fault(operation);
}

static void *output_malloc(size_t size)
{
    return output_fault("output_malloc") ? NULL : malloc(size);
}
static int output_mkstemp(char *template)
{
    return output_fault("output_mkstemp") ? -1 : mkstemp(template);
}
static FILE *output_fdopen(int fd, const char *mode)
{
    return output_fault("output_fdopen") ? NULL : fdopen(fd, mode);
}
static int output_ferror(FILE *fp)
{
    return output_fault("output_ferror") ? 1 : ferror(fp);
}
static int output_fclose(FILE *fp)
{
    int result = fclose(fp);
    return output_fault("output_fclose") ? EOF : result;
}
static int output_fflush(FILE *fp)
{
    int result = fflush(fp);
    return output_fault("output_fflush") ? EOF : result;
}
static int output_fchmod(int fd, mode_t mode)
{
    return output_fault("output_fchmod") ? -1 : fchmod(fd, mode);
}
static int output_rename(const char *from, const char *to)
{
    return output_fault("output_rename") ? -1 : rename(from, to);
}
#define malloc output_malloc
#define mkstemp output_mkstemp
#define fdopen output_fdopen
#define ferror output_ferror
#define fclose output_fclose
#define fflush output_fflush
#define fchmod output_fchmod
#define rename output_rename
#define output_file_open output_file_open_impl
#define output_file_close output_file_close_impl
#define output_file_publish output_file_publish_impl
#define output_file_finish output_file_finish_impl
#define output_file_discard output_file_discard_impl
#include "../output_file.c"
#undef malloc
#undef mkstemp
#undef fdopen
#undef ferror
#undef fclose
#undef fflush
#undef fchmod
#undef rename
#undef output_file_open
#undef output_file_close
#undef output_file_publish
#undef output_file_finish
#undef output_file_discard

int output_file_open(output_writer *output, const char *path)
{
    int result;
    fault_output_path = path;
    result = output_file_open_impl(output, path);
    fault_output_path = NULL;
    return result;
}
int output_file_close(output_writer *output, int success)
{
    int result;
    fault_output_path = output->path;
    result = output_file_close_impl(output, success);
    fault_output_path = NULL;
    return result;
}
int output_file_publish(output_writer *output)
{
    int result;
    fault_output_path = output->path;
    result = output_file_publish_impl(output);
    fault_output_path = NULL;
    return result;
}
int output_file_finish(output_writer *output, int success)
{
    int result;
    fault_output_path = output->path;
    result = output_file_finish_impl(output, success);
    fault_output_path = NULL;
    return result;
}
void output_file_discard(output_writer *output)
{
    fault_output_path = output->path;
    output_file_discard_impl(output);
    fault_output_path = NULL;
}

#include "../fceux_nl.c"
