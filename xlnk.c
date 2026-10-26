/* Linker driver: checked objects, shared layout and transactional output. */
#include "ir/path.h"
#include "getopt.h"
#include "output_file.h"
#include "script.h"
#include "version.h"
#include "ir/link.h"
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int err_count, warn_count;
typedef struct
{
    const char *path;
    ir_id boundary;
    output_writer writer;
} destination;
static void diagnostic(const char *file, unsigned line, const char *format, ...)
{
    va_list args;
    if (file)
        fprintf(stderr, "%s:%u: ", file, line);
    fputs("error: ", stderr);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    ++err_count;
}
static ir_location source_location(ir_id file, int line)
{
    ir_location l = {file, (uint32_t)line, 1, (uint32_t)line, 1};
    return l;
}
static const char *required(xlnk_script *s, xlnk_script_command *c, const char *key)
{
    const char *value = xlnk_script_get_command_arg(c, key);
    if (!value)
        diagnostic(s->name, (unsigned)c->line, "missing argument `%s'", key);
    return value;
}
static int number(xlnk_script *s, xlnk_script_command *c, const char *key, uint32_t *value,
                  int need)
{
    const char *text = need ? required(s, c, key) : xlnk_script_get_command_arg(c, key);
    char *end;
    unsigned long long n;
    int base = 0;
    if (!text)
        return !need;
    if (*text == '$')
    {
        ++text;
        base = 16;
    }
    else if (*text == '%')
    {
        ++text;
        base = 2;
    }
    errno = 0;
    n = strtoull(text, &end, base);
    if (errno || !*text || *end || *text == '-' || n > UINT32_MAX)
    {
        diagnostic(s->name, (unsigned)c->line, "invalid value of `%s'", key);
        return 0;
    }
    *value = (uint32_t)n;
    return 1;
}
static ir_object *read_object(const char *path, ir_heap *heap)
{
    FILE *f = fopen(path, "rb");
    unsigned char *bytes = NULL;
    long size;
    ir_object *o = NULL;
    ir_object_error error = IR_OBJECT_FORMAT;
    if (!f)
    {
        diagnostic(path, 0, "could not open object");
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) || (size = ftell(f)) < 0 || (uintmax_t)size > UINT32_MAX ||
        fseek(f, 0, SEEK_SET))
        goto done;
    bytes = ir_malloc(heap, size ? (size_t)size : 1);
    if (!bytes)
    {
        error = IR_OBJECT_MEMORY;
        goto done;
    }
    if (fread(bytes, 1, (size_t)size, f) != (size_t)size)
        goto done;
    if (size >= 2 && bytes[0] == 0xfa && bytes[1] == 0xce)
    {
        diagnostic(path, 0, "v1 object is unsupported; rebuild with xasm");
        ir_free(heap, bytes);
        fclose(f);
        return NULL;
    }
    o = ir_object_decode_heap_owned(heap, bytes, (size_t)size, &error);
    bytes = NULL;
done:
    ir_free(heap, bytes);
    if (fclose(f))
    {
        ir_object_destroy(o);
        o = NULL;
    }
    if (!o)
        diagnostic(path, 0, "%s", ir_object_error_message(error));
    return o;
}
/* realpath needs the leaf to exist. Resolving its parent also identifies
 * duplicate destinations before either file has been created. */
static char *destination_path(const char *path)
{
    char *copy, *slash, *parent, *result;
    const char *leaf;
    size_t n, m;
    result = realpath(path, NULL);
    if (result)
        return result;
    copy = strdup(path);
    if (!copy)
        return NULL;
    slash = strrchr(copy, '/');
    leaf = slash ? slash + 1 : path;
    if (slash)
    {
        *slash = 0;
        parent = realpath(*copy ? copy : "/", NULL);
    }
    else
        parent = realpath(".", NULL);
    if (!parent)
    {
        free(copy);
        return NULL;
    }
    n = strlen(parent);
    m = strlen(leaf);
    result = n <= SIZE_MAX - m - 2 ? malloc(n + m + 2) : NULL;
    if (result)
    {
        memcpy(result, parent, n);
        result[n] = '/';
        memcpy(result + n + 1, leaf, m + 1);
    }
    free(parent);
    free(copy);
    return result;
}
static int aliases(const char *a, const char *b)
{
    struct stat x, y;
    char *ra, *rb;
    int same, ax, by, a_missing, b_missing;
    if (!strcmp(a, b))
        return 1;
    ax = stat(a, &x);
    a_missing = ax && (errno == ENOENT || errno == ENOTDIR);
    by = stat(b, &y);
    b_missing = by && (errno == ENOENT || errno == ENOTDIR);
    if (!ax && !by)
        return x.st_dev == y.st_dev && x.st_ino == y.st_ino;
    /* An existing input and a missing destination cannot name the same file.
     * Canonical parent paths are needed for two not-yet-created outputs. */
    if ((!ax && b_missing) || (!by && a_missing))
        return 0;
    ra = destination_path(a);
    rb = destination_path(b);
    same = ra && rb && !strcmp(ra, rb);
    if (ra && rb && !same) {
        char *a_slash = strrchr(ra, '/'), *b_slash = strrchr(rb, '/');
        *a_slash = *b_slash = 0;
        if (!stat(*ra ? ra : "/", &x) && !stat(*rb ? rb : "/", &y) &&
            x.st_dev == y.st_dev && x.st_ino == y.st_ino) {
            const char *error;
            same = ir_same_future_name(*ra ? ra : "/", a_slash + 1, b_slash + 1, &error);
            if (error) diagnostic(a, 0, "%s", error);
        }
    }
    free(ra);
    free(rb);
    return same;
}
static int protect_outputs(xlnk_script *s, destination *dest, size_t count)
{
    size_t i, j;
    xlnk_script_command *c;
    for (i = 0; i < count; i++)
    {
        if (aliases(dest[i].path, s->name))
        {
            diagnostic(dest[i].path, 0, "output aliases linker script");
            return 0;
        }
        for (j = 0; j < i; j++)
            if (aliases(dest[i].path, dest[j].path))
            {
                diagnostic(dest[i].path, 0, "duplicate output destination");
                return 0;
            }
        for (c = s->first_command; c; c = c->next)
            if (c->type == XLNK_LINK_COMMAND || c->type == XLNK_COPY_COMMAND)
            {
                const char *path = xlnk_script_get_command_arg(c, "file");
                if (path && aliases(dest[i].path, path))
                {
                    diagnostic(dest[i].path, 0, "output aliases input `%s'", path);
                    return 0;
                }
            }
    }
    return 1;
}
int main(int argc, char **argv)
{
    static struct option options[] = {
        {"output", required_argument, 0, 'o'}, {"quiet", no_argument, 0, 'q'},
        {"silent", no_argument, 0, 's'},       {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},         {"usage", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},      {0, 0, 0, 0}};
    int key, quiet = 0, verbose = 0, status = 1;
    const char *default_output = NULL;
    ir_heap heap = {0};
    xlnk_script script = {0};
    xlnk_script_command *c;
    ir_link *link = NULL;
    ir_layout *layout;
    ir_unit *values;
    ir_id file;
    destination *dest = NULL;
    size_t count = 0, capacity = 0, i, fact_count;
    ir_layout_fact fact;
    uint32_t bank_origin = 0, bank_size = 0;
    while ((key = getopt_long(argc, argv, "o:qsvVh", options, NULL)) != -1)
    {
        if (key == 'o')
            default_output = optarg;
        else if (key == 'q' || key == 's')
            quiet = 1, verbose = 0;
        else if (key == 'v')
            quiet = 0, verbose = 1;
        else if (key == 'V')
        {
            puts("xlnk " XORCYST_VERSION "");
            return 0;
        }
        else if (key == 'h')
        {
            puts("Usage: xlnk [OPTION...] SCRIPT\n"
                 "Link XOROBJ2 objects using SCRIPT. Rebuild older objects with xasm.\n"
                 "\n"
                 "  -o, --output=FILE          Set the default output file\n"
                 "  -q, -s, --quiet, --silent  Suppress warnings and progress output\n"
                 "  -v, --verbose             Report linked, copied and output files\n"
                 "  -h, --help, --usage        Display this help and exit\n"
                 "  -V, --version             Print the program version and exit\n"
                 "\n"
                 "Arguments required by long options are also required by their short forms.");
            return 0;
        }
        else
            return 1;
    }
    if (optind + 1 != argc)
    {
        diagnostic(NULL, 0, "expected one linker script");
        return 1;
    }
    if (!xlnk_script_parse(argv[optind], &script))
        goto done;
    link = ir_link_create_heap(&heap);
    if (!link)
    {
        diagnostic(NULL, 0, "out of memory");
        goto done;
    }
    layout = ir_link_layout(link);
    values = ir_link_values(link);
    file = ir_file(values, script.name);
    for (c = script.first_command; c && !err_count; c = c->next)
    {
        if (c->type == XLNK_LINK_COMMAND)
            required(&script, c, "file");
        else if (c->type == XLNK_RAM_COMMAND)
        {
            uint32_t start = 0, end = 0;
            if (number(&script, c, "start", &start, 1) && number(&script, c, "end", &end, 1))
                if (start >= end || end > 65535 || !ir_layout_ram_block(layout, start, end))
                    diagnostic(script.name, (unsigned)c->line, "invalid RAM block");
        }
        if (c->type == XLNK_OUTPUT_COMMAND)
            ++capacity;
    }
    if (err_count)
        goto done;
    if (default_output)
        ++capacity;
    dest = calloc(capacity ? capacity : 1, sizeof(*dest));
    if (!dest)
    {
        diagnostic(NULL, 0, "out of memory");
        goto done;
    }
    if (default_output)
    {
        dest[count].path = default_output;
        dest[count++].boundary =
            ir_layout_mark(layout, ir_anchor(values, ir_no_location()), ir_no_location());
    }
    if (ir_layout_error_at(layout).code)
        goto link_error;
    for (c = script.first_command; c && !err_count; c = c->next)
    {
        ir_location loc = source_location(file, c->line);
        if (c->type == XLNK_OUTPUT_COMMAND)
        {
            dest[count].path = required(&script, c, "file");
            dest[count++].boundary = ir_layout_mark(layout, ir_anchor(values, loc), loc);
        }
        else if (c->type == XLNK_LINK_COMMAND)
        {
            uint32_t origin = 0;
            if (!count)
            {
                diagnostic(script.name, (unsigned)c->line, "no output open");
                break;
            }
            if (xlnk_script_get_command_arg(c, "origin"))
            {
                if (!number(&script, c, "origin", &origin, 1))
                    goto done;
                if (origin > 65535)
                {
                    diagnostic(script.name, (unsigned)c->line,
                               "value of argument `origin' is out of range");
                    goto done;
                }
                if (ir_layout_origin(layout, origin, loc) == IR_NONE)
                    goto link_error;
            }
            {
                const char *path = required(&script, c, "file");
                ir_object *o = path ? read_object(path, &heap) : NULL;
                ir_id unit;
                if (!o)
                    goto done;
                if (verbose)
                    printf("link: %s\n", path);
                unit = ir_link_add(link, o, path);
                if (unit == IR_NONE || !ir_link_append(link, unit))
                    goto link_error;
            }
        }
        else if (c->type == XLNK_COPY_COMMAND)
        {
            const char *path = required(&script, c, "file");
            if (!count)
            {
                diagnostic(script.name, (unsigned)c->line, "no output open");
                break;
            }
            if (verbose && path)
                printf("copy: %s\n", path);
            if (path && ir_layout_copy(layout, path, loc) == IR_NONE)
            {
                if (ir_layout_error_at(layout).code == IR_LAYOUT_INPUT)
                {
                    diagnostic(script.name, (unsigned)c->line,
                               "`copy': could not read `%s'", path);
                    goto done;
                }
                goto link_error;
            }
        }
        else if (c->type == XLNK_BANK_COMMAND)
        {
            number(&script, c, "origin", &bank_origin, 0);
            number(&script, c, "size", &bank_size, 0);
            if (!bank_size || bank_origin > 65535)
                diagnostic(script.name, (unsigned)c->line, "invalid bank size or origin");
            else
                ir_layout_bank(layout, bank_origin, bank_size, loc);
        }
        else if (c->type == XLNK_PAD_COMMAND)
        {
            uint32_t n = 0;
            const char *arg = "size";
            ir_pad_kind kind = IR_PAD_SIZE;
            if (!count)
            {
                diagnostic(script.name, (unsigned)c->line, "no output open");
                break;
            }
            if (xlnk_script_get_command_arg(c, "offset"))
            {
                arg = "offset";
                kind = IR_PAD_BANK_OFFSET;
            }
            else if (xlnk_script_get_command_arg(c, "origin"))
            {
                arg = "origin";
                kind = IR_PAD_ORIGIN;
            }
            if (number(&script, c, arg, &n, 1))
                ir_layout_pad(layout, kind, n, loc);
        }
        if (ir_layout_error_at(layout).code)
            goto link_error;
    }
    if (err_count || !protect_outputs(&script, dest, count))
        goto done;
    if (!ir_layout_keep_bytes(layout, 0) || !ir_link_finish(link))
        goto link_error;
    fact_count = ir_layout_fact_count(layout);
    for (i = 0; i < fact_count; i++)
        if (ir_layout_fact_at(layout, (ir_id)i, &fact) && fact.warning)
        {
            ir_location loc = fact.location;
            const char *name = ir_file_name(values, loc.file);
            if (!quiet)
                fprintf(stderr, "%s:%u: warning: operand out of range; truncated\n",
                        name ? name : "location unavailable", loc.line);
            ++warn_count;
        }
    /* Stage and close every file before publishing any. Layout and all input
     * validation have completed; failures leave existing outputs untouched. */
    for (i = 0; i < count; i++)
    {
        size_t first = dest[i].boundary;
        size_t end = i + 1 < count ? dest[i + 1].boundary : fact_count;
        if (first > end || end > fact_count)
            goto io_error;
        if (!output_file_open(&dest[i].writer, dest[i].path))
            goto io_error;
        if (!ir_layout_write_fragments(layout, first, end, dest[i].writer.stream))
            goto io_error;
        if (!output_file_close(&dest[i].writer, 1))
            goto io_error;
    }
    for (i = 0; i < count; i++)
    {
        if (!output_file_publish(&dest[i].writer))
            goto io_error;
        if (verbose)
            printf("output: %s\n", dest[i].path);
    }
    status = 0;
    goto done;
link_error:
    {
        ir_location loc = ir_link_error_location(link);
        const char *name = ir_file_name(values, loc.file);
        const char *symbol = ir_link_error_symbol(link);
        if (symbol)
            diagnostic(name ? name : "location unavailable", loc.line,
                       "%s `%s'", ir_link_error(link), symbol);
        else
            diagnostic(name ? name : "location unavailable", loc.line,
                       "%s", ir_link_error(link));
        goto done;
    }
io_error:
    diagnostic(NULL, 0, "could not publish linker output");
    status = 3;
done:
    for (i = 0; i < count; i++)
        output_file_discard(&dest[i].writer);
    free(dest);
    ir_link_destroy(link);
    ir_heap_destroy(&heap);
    xlnk_script_finalize(&script);
    return status;
}
