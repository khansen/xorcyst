/* Non-installed v2 linker driver. V1 script grammar, strict numeric arguments. */
#include "path.h"
#include "../getopt.h"
#include "../output_file.h"
#include "../script.h"
#include "../version.h"
#include "link.h"
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
    v2_id boundary;
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
static v2_location source_location(v2_id file, int line)
{
    v2_location l = {file, (uint32_t)line, 1, (uint32_t)line, 1};
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
static v2_object *read_object(const char *path, v2_heap *heap)
{
    FILE *f = fopen(path, "rb");
    unsigned char *bytes = NULL;
    long size;
    v2_object *o = NULL;
    v2_object_error error = V2_OBJECT_FORMAT;
    if (!f)
    {
        diagnostic(path, 0, "could not open object");
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) || (size = ftell(f)) < 0 || (uintmax_t)size > UINT32_MAX ||
        fseek(f, 0, SEEK_SET))
        goto done;
    bytes = v2_malloc(heap, size ? (size_t)size : 1);
    if (!bytes)
    {
        error = V2_OBJECT_MEMORY;
        goto done;
    }
    if (fread(bytes, 1, (size_t)size, f) != (size_t)size)
        goto done;
    if (size >= 2 && bytes[0] == 0xfa && bytes[1] == 0xce)
    {
        diagnostic(path, 0, "v1 object is unsupported; rebuild with xasm-v2");
        v2_free(heap, bytes);
        fclose(f);
        return NULL;
    }
    o = v2_object_decode_heap_owned(heap, bytes, (size_t)size, &error);
    bytes = NULL;
done:
    v2_free(heap, bytes);
    if (fclose(f))
    {
        v2_object_destroy(o);
        o = NULL;
    }
    if (!o)
        diagnostic(path, 0, "%s", v2_object_error_message(error));
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
            same = v2_same_future_name(*ra ? ra : "/", a_slash + 1, b_slash + 1, &error);
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
    int key, quiet = 0, status = 1;
    const char *default_output = NULL;
    v2_heap heap = {0};
    xlnk_script script = {0};
    xlnk_script_command *c;
    v2_link *link = NULL;
    v2_layout *layout;
    v2_unit *values;
    v2_id file;
    destination *dest = NULL;
    size_t count = 0, capacity = 0, i, fact_count;
    v2_layout_fact fact;
    uint32_t bank_origin = 0, bank_size = 0;
    while ((key = getopt_long(argc, argv, "o:qsvVh", options, NULL)) != -1)
    {
        if (key == 'o')
            default_output = optarg;
        else if (key == 'q' || key == 's')
            quiet = 1;
        else if (key == 'v')
            quiet = 0;
        else if (key == 'V')
        {
            puts("xlnk-v2 " XORCYST_VERSION " (I4 prototype)");
            return 0;
        }
        else if (key == 'h')
        {
            puts("Usage: xlnk-v2 [-qsv] [-o FILE] SCRIPT\nLinks XOROBJ2 objects; v1 objects are "
                 "not supported.");
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
    link = v2_link_create_heap(&heap);
    if (!link)
    {
        diagnostic(NULL, 0, "out of memory");
        goto done;
    }
    layout = v2_link_layout(link);
    values = v2_link_values(link);
    file = v2_file(values, script.name);
    for (c = script.first_command; c && !err_count; c = c->next)
    {
        if (c->type == XLNK_LINK_COMMAND)
            required(&script, c, "file");
        else if (c->type == XLNK_RAM_COMMAND)
        {
            uint32_t start = 0, end = 0;
            if (number(&script, c, "start", &start, 1) && number(&script, c, "end", &end, 1))
                if (start >= end || end > 65535 || !v2_layout_ram_block(layout, start, end))
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
            v2_layout_mark(layout, v2_anchor(values, v2_no_location()), v2_no_location());
    }
    if (v2_layout_error_at(layout).code)
        goto link_error;
    for (c = script.first_command; c && !err_count; c = c->next)
    {
        v2_location loc = source_location(file, c->line);
        if (c->type == XLNK_OUTPUT_COMMAND)
        {
            dest[count].path = required(&script, c, "file");
            dest[count++].boundary = v2_layout_mark(layout, v2_anchor(values, loc), loc);
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
                if (v2_layout_origin(layout, origin, loc) == V2_NONE)
                    goto link_error;
            }
            {
                const char *path = required(&script, c, "file");
                v2_object *o = path ? read_object(path, &heap) : NULL;
                v2_id unit;
                if (!o)
                    goto done;
                unit = v2_link_add(link, o, path);
                if (unit == V2_NONE || !v2_link_append(link, unit))
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
            if (path && v2_layout_copy(layout, path, loc) == V2_NONE)
            {
                if (v2_layout_error_at(layout).code == V2_LAYOUT_INPUT)
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
                v2_layout_bank(layout, bank_origin, bank_size, loc);
        }
        else if (c->type == XLNK_PAD_COMMAND)
        {
            uint32_t n = 0;
            const char *arg = "size";
            v2_pad_kind kind = V2_PAD_SIZE;
            if (!count)
            {
                diagnostic(script.name, (unsigned)c->line, "no output open");
                break;
            }
            if (xlnk_script_get_command_arg(c, "offset"))
            {
                arg = "offset";
                kind = V2_PAD_BANK_OFFSET;
            }
            else if (xlnk_script_get_command_arg(c, "origin"))
            {
                arg = "origin";
                kind = V2_PAD_ORIGIN;
            }
            if (number(&script, c, arg, &n, 1))
                v2_layout_pad(layout, kind, n, loc);
        }
        if (v2_layout_error_at(layout).code)
            goto link_error;
    }
    if (err_count || !protect_outputs(&script, dest, count))
        goto done;
    if (!v2_layout_keep_bytes(layout, 0) || !v2_link_finish(link))
        goto link_error;
    fact_count = v2_layout_fact_count(layout);
    for (i = 0; i < fact_count; i++)
        if (v2_layout_fact_at(layout, (v2_id)i, &fact) && fact.warning)
        {
            v2_location loc = fact.location;
            const char *name = v2_file_name(values, loc.file);
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
        if (!v2_layout_write_fragments(layout, first, end, dest[i].writer.stream))
            goto io_error;
        if (!output_file_close(&dest[i].writer, 1))
            goto io_error;
    }
    for (i = 0; i < count; i++)
        if (!output_file_publish(&dest[i].writer))
            goto io_error;
    status = 0;
    goto done;
link_error:
    {
        v2_location loc = v2_link_error_location(link);
        const char *name = v2_file_name(values, loc.file);
        const char *symbol = v2_link_error_symbol(link);
        if (symbol)
            diagnostic(name ? name : "location unavailable", loc.line,
                       "%s `%s'", v2_link_error(link), symbol);
        else
            diagnostic(name ? name : "location unavailable", loc.line,
                       "%s", v2_link_error(link));
        goto done;
    }
io_error:
    diagnostic(NULL, 0, "could not publish linker output");
    status = 3;
done:
    for (i = 0; i < count; i++)
        output_file_discard(&dest[i].writer);
    free(dest);
    v2_link_destroy(link);
    v2_heap_destroy(&heap);
    xlnk_script_finalize(&script);
    return status;
}
