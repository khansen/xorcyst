#include "dependencies.h"
#include "sha256.h"
#include "utf8.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

typedef struct dependency {
    char *path;
    unsigned roles;
    int missing;
    unsigned char *bytes;
    size_t size;
    char hash[65];
    struct dependency *next;
} dependency;

static dependency *inputs, **tail = &inputs;
static int active, protect_outputs, failed, argument_count;
static char **arguments, *directory;
static const char *producer_version;
static char *manifest_path;
static dependency *outputs;
static dependency *source_paths;
static int aliases_input(const char *path);

static void failure(const char *message, const char *path)
{
    fprintf(stderr, "error: %s: %s%s%s\n", active ? "dependency manifest" : "output protection", message,
            path != NULL ? ": " : "", path != NULL ? path : "");
    failed = 1;
}

static char *absolute_path(const char *path)
{
    char *result;
    if (!path || !directory || !xasm_utf8_valid(path, strlen(path))) {
        failure("path is unavailable or not UTF-8", path);
        return NULL;
    }
    if (path[0] == '/') result = strdup(path);
    else {
        result = malloc(strlen(directory) + strlen(path) + 2);
        if (result) sprintf(result, "%s/%s", directory, path);
    }
    if (!result) failure("out of memory", path);
    return result;
}

/* Preserve the lookup path (including symlink components) rather than replacing
 * it with realpath: a later symlink retarget must be checked through that path. */
static dependency *find_input(const char *path)
{
    dependency *entry;
    for (entry = inputs; entry; entry = entry->next)
        if (strcmp(entry->path, path) == 0) return entry;
    return NULL;
}

/* Keep original lookup paths for input validation. Resolve prospective output
 * paths separately, including a final symlink whose target is not yet present. */
static char *resolved_parent(const char *path, const char *slash)
{
    size_t length = slash == path ? 1 : (size_t)(slash - path);
    char *parent = malloc(length + 1);
    char *resolved;
    if (parent == NULL) { failure("out of memory comparing output paths", path); return NULL; }
    memcpy(parent, path, length);
    parent[length] = '\0';
    resolved = realpath(parent, NULL);
    if (resolved == NULL && errno != ENOENT && errno != ENOTDIR)
        failure("cannot resolve output directory", path);
    free(parent);
    return resolved;
}

static char *resolved_output_path(const char *path, unsigned links)
{
    struct stat info;
    char *resolved = realpath(path, NULL);
    const char *slash;
    char *parent;
    size_t length;
    if (resolved != NULL) return resolved;
    if (errno != ENOENT && errno != ENOTDIR) {
        failure("cannot resolve output path", path);
        return NULL;
    }
    slash = strrchr(path, '/');
    if (slash == NULL) { failure("output path is not absolute", path); return NULL; }
    if (lstat(path, &info) == 0 && S_ISLNK(info.st_mode)) {
        char *target, *next;
        size_t capacity = 128;
        ssize_t count;
        if (links >= 40) { failure("cannot resolve output symlink chain", path); return NULL; }
        for (;;) {
            target = malloc(capacity + 1);
            if (target == NULL) { failure("out of memory", path); return NULL; }
            count = readlink(path, target, capacity);
            if (count < 0) { free(target); failure("cannot resolve output symlink", path); return NULL; }
            if ((size_t)count < capacity) break;
            free(target);
            if (capacity > (SIZE_MAX - 1) / 2) { failure("output symlink is too long", path); return NULL; }
            capacity *= 2;
        }
        target[count] = '\0';
        if (target[0] == '/') next = target;
        else {
            size_t prefix = (size_t)(slash - path) + 1;
            next = malloc(prefix + (size_t)count + 1);
            if (next != NULL) {
                memcpy(next, path, prefix);
                memcpy(next + prefix, target, (size_t)count + 1);
            }
            free(target);
        }
        if (next == NULL) { failure("out of memory", path); return NULL; }
        resolved = resolved_output_path(next, links + 1);
        free(next);
        return resolved;
    }
    parent = resolved_parent(path, slash);
    if (parent == NULL) return NULL; /* A missing parent cannot accept an output. */
    length = strlen(parent) + strlen(slash) + 1;
    resolved = malloc(length);
    if (resolved != NULL) snprintf(resolved, length, "%s%s", parent, slash);
    else failure("out of memory", path);
    free(parent);
    return resolved;
}

static int same_future_name(const char *parent, const char *a, const char *b)
{
    if (strcmp(a, b) == 0) return 1;
#ifdef __APPLE__
    /* Darwin filesystems expose case sensitivity; CoreFoundation supplies
       Unicode comparison without changing the assembler's process locale. */
    long sensitive = pathconf(parent, _PC_CASE_SENSITIVE);
    CFStringRef left, right;
    CFStringCompareFlags flags = kCFCompareNonliteral;
    int same;
    if (sensitive < 0) { failure("cannot determine output filesystem case sensitivity", parent); return 1; }
    if (!sensitive) flags |= kCFCompareCaseInsensitive;
    left = CFStringCreateWithCString(NULL, a, kCFStringEncodingUTF8);
    right = CFStringCreateWithCString(NULL, b, kCFStringEncodingUTF8);
    same = left == NULL || right == NULL || CFStringCompare(left, right, flags) == kCFCompareEqualTo;
    if (left == NULL || right == NULL) failure("out of memory comparing output names", parent);
    if (left != NULL) CFRelease(left);
    if (right != NULL) CFRelease(right);
    return same;
#else
    (void)parent;
    return 0;
#endif
}

static int same_file(const char *a, const char *b)
{
    struct stat sa, sb;
    char *left, *right, *a_slash, *b_slash;
    int same = 0;
    if (strcmp(a, b) == 0) return 1;
    if (stat(a, &sa) == 0 && stat(b, &sb) == 0)
        return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
    left = resolved_output_path(a, 0);
    right = resolved_output_path(b, 0);
    if (left != NULL && right != NULL) {
        a_slash = strrchr(left, '/');
        b_slash = strrchr(right, '/');
        *a_slash = *b_slash = '\0';
        if (stat(left[0] ? left : "/", &sa) == 0 && stat(right[0] ? right : "/", &sb) == 0
            && sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino)
            same = same_future_name(left[0] ? left : "/", a_slash + 1, b_slash + 1);
    }
    free(left);
    free(right);
    same |= failed;
    return same;
}

static int read_contents(const char *path, unsigned char **bytes, size_t *size, char hash[65])
{
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    FILE *fp;
    struct stat statbuf;
    unsigned char buffer[16384], *data = NULL;
    size_t length = 0, capacity = 0, count;
    sha256_context ctx;
    if (fd < 0) return 0;
    if (fstat(fd, &statbuf) != 0 || !S_ISREG(statbuf.st_mode)) {
        close(fd);
        errno = EINVAL;
        return 0;
    }
    fp = fdopen(fd, "rb");
    if (!fp) { close(fd); return 0; }
    sha256_init(&ctx);
    while ((count = fread(buffer, 1, sizeof(buffer), fp)) != 0) {
        unsigned char *next;
        if (count > SIZE_MAX - length) { errno = EOVERFLOW; goto fail; }
        if (length + count > capacity) {
            size_t needed = length + count;
            capacity = capacity > SIZE_MAX / 2 ? needed : capacity * 2;
            if (capacity < needed) capacity = needed;
            next = realloc(data, capacity);
            if (!next) goto fail;
            data = next;
        }
        memcpy(data + length, buffer, count);
        length += count;
        sha256_update(&ctx, buffer, count);
    }
    if (ferror(fp)) { errno = EIO; goto fail; }
    if (fclose(fp) != 0) { free(data); return 0; }
    sha256_finish(&ctx, hash);
    *bytes = data;
    *size = length;
    return 1;
fail:
    fclose(fp);
    free(data);
    return 0;
}

static dependency *capture(const char *path, unsigned role)
{
    dependency *entry;
    char *absolute = absolute_path(path);
    if (!absolute) return NULL;
    if (same_file(absolute, manifest_path)) {
        failure("manifest output aliases an input or lookup probe", absolute);
        free(absolute);
        return NULL;
    }
    for (entry = outputs; entry; entry = entry->next) if (same_file(absolute, entry->path)) {
        failure("input aliases an output", absolute);
        free(absolute);
        return NULL;
    }
    entry = find_input(absolute);
    if (entry) {
        free(absolute);
        entry->roles |= role;
        return entry;
    }
    entry = calloc(1, sizeof(*entry));
    if (!entry) { free(absolute); failure("out of memory", path); return NULL; }
    entry->path = absolute;
    entry->roles = role;
    if (!read_contents(absolute, &entry->bytes, &entry->size, entry->hash)) {
        if (errno == ENOENT || errno == ENOTDIR) entry->missing = 1;
        else failure("cannot snapshot regular input", absolute);
    }
    *tail = entry;
    tail = &entry->next;
    return entry;
}

void dependencies_arguments(int argc, char **argv)
{
    int i;
    argument_count = argc;
    arguments = calloc((size_t)argc, sizeof(*arguments));
    if (!arguments) return;
    for (i = 0; i < argc; i++) arguments[i] = strdup(argv[i]);
}

static char *executable_path(void)
{
#ifdef __APPLE__
    uint32_t size = 256;
    char *path = malloc(size);
    if (!path) return NULL;
    if (_NSGetExecutablePath(path, &size) != 0) {
        free(path);
        path = malloc(size);
        if (!path) return NULL;
        if (_NSGetExecutablePath(path, &size) != 0) { free(path); return NULL; }
    }
    return path;
#elif defined(__linux__)
    size_t size = 256;
    for (;;) {
        char *path = malloc(size + 1);
        ssize_t count;
        if (!path) return NULL;
        count = readlink("/proc/self/exe", path, size);
        if (count < 0) { free(path); return NULL; }
        if ((size_t)count < size) { path[count] = '\0'; return path; }
        free(path);
        if (size > SIZE_MAX / 2 - 1) return NULL;
        size *= 2;
    }
#else
    return NULL;
#endif
}

int dependencies_start_output_protection(void)
{
    protect_outputs = 1;
    directory = getcwd(NULL, 0);
    if (directory == NULL) failure("cannot identify working directory", NULL);
    return !failed;
}

int dependencies_start(const char *version, const char *manifest)
{
    int i;
    char *executable;
    dependency *entry;
    active = 1;
    producer_version = version;
    directory = getcwd(NULL, 0);
    if (!directory || !xasm_utf8_valid(directory, strlen(directory)) || !arguments) {
        failure("cannot identify invocation", NULL);
        return 0;
    }
    for (i = 0; i < argument_count; i++) {
        if (!arguments[i] || !xasm_utf8_valid(arguments[i], strlen(arguments[i]))) {
            failure("argument is unavailable or not UTF-8", NULL);
            return 0;
        }
    }
    manifest_path = absolute_path(manifest);
    if (!manifest_path) return 0;
    executable = executable_path();
    if (!executable) { failure("cannot identify running producer", NULL); return 0; }
    entry = capture(executable, DEP_PRODUCER);
    free(executable);
    if (!entry || entry->missing) failure("cannot hash running producer", NULL);
    return !failed;
}

FILE *dependencies_open(const char *path, const char *mode, unsigned role)
{
    dependency *entry;
    FILE *fp;
    if (!active) {
        if (protect_outputs && !dependencies_protect_source(path)) return NULL;
        return fopen(path, mode);
    }
    entry = capture(path, role);
    if (!entry || failed || entry->missing) { errno = ENOENT; return NULL; }
    fp = tmpfile();
    if (!fp) { failure("cannot open input snapshot", path); return NULL; }
    if (fwrite(entry->bytes, 1, entry->size, fp) != entry->size || fflush(fp) != 0
        || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        failure("cannot prepare input snapshot", path);
        return NULL;
    }
    return fp;
}

int dependencies_validate(void)
{
    dependency *entry;
    if (!active) return 1;
    if (failed) return 0;
    for (entry = inputs; entry; entry = entry->next) {
        unsigned char *bytes = NULL;
        size_t size = 0;
        char hash[65];
        int exists = read_contents(entry->path, &bytes, &size, hash);
        int missing = !exists && (errno == ENOENT || errno == ENOTDIR);
        free(bytes);
        if (entry->missing ? !missing : (!exists || size != entry->size || strcmp(hash, entry->hash))) {
            failure("input changed or became unavailable", entry->path);
            return 0;
        }
    }
    for (entry = outputs; entry; entry = entry->next) {
        dependency *other;
        if (same_file(entry->path, manifest_path) || aliases_input(entry->path)) {
            failure("output aliases an input or manifest", entry->path);
            return 0;
        }
        for (other = entry->next; other; other = other->next)
            if (same_file(entry->path, other->path)) {
                failure("output paths alias each other", entry->path);
                return 0;
            }
    }
    return 1;
}

static void json_string(FILE *fp, const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    fputc('"', fp);
    for (; *p; p++) {
        if (*p == '"' || *p == '\\') { fputc('\\', fp); fputc(*p, fp); }
        else if (*p < 0x20) fprintf(fp, "\\u%04x", *p);
        else fputc(*p, fp);
    }
    fputc('"', fp);
}

static int aliases_input(const char *path)
{
    dependency *entry;
    for (entry = inputs; entry; entry = entry->next)
        if (same_file(path, entry->path)) return 1;
    for (entry = source_paths; entry; entry = entry->next)
        if (same_file(path, entry->path)) return 1;
    return 0;
}

int dependencies_protect_source(const char *path)
{
    dependency *entry;
    char *absolute;
    if ((!active && !protect_outputs) || !path) return 1;
    absolute = absolute_path(path);
    if (!absolute) return 0;
    for (entry = outputs; entry; entry = entry->next) {
        if (same_file(absolute, entry->path)) {
            failure("input aliases an output", absolute);
            free(absolute);
            return 0;
        }
    }
    for (entry = source_paths; entry; entry = entry->next)
        if (strcmp(entry->path, absolute) == 0) { free(absolute); return 1; }
    entry = calloc(1, sizeof(*entry));
    if (!entry) { free(absolute); failure("out of memory", path); return 0; }
    entry->path = absolute;
    entry->next = source_paths;
    source_paths = entry;
    return 1;
}

int dependencies_output(const char *path)
{
    dependency *entry;
    char *absolute;
    if ((!active && !protect_outputs) || !path) return 1;
    absolute = absolute_path(path);
    if (!absolute) return 0;
    if ((manifest_path != NULL && same_file(absolute, manifest_path)) || aliases_input(absolute)) goto collision;
    for (entry = outputs; entry; entry = entry->next)
        if (same_file(absolute, entry->path)) goto collision;
    entry = calloc(1, sizeof(*entry));
    if (!entry) { free(absolute); failure("out of memory", path); return 0; }
    entry->path = absolute;
    entry->next = outputs;
    outputs = entry;
    return 1;
collision:
    failure("output aliases an input, lookup probe, or another output", absolute);
    free(absolute);
    return 0;
}

int dependencies_write(const char *path)
{
    char *absolute, *temporary;
    FILE *fp;
    int fd, ok, i, first;
    dependency *entry;
    if (!active || !dependencies_validate()) return 0;
    absolute = absolute_path(path);
    if (!absolute) return 0;
    if (aliases_input(absolute)) {
        failure("manifest output aliases an input or lookup probe", absolute);
        free(absolute);
        return 0;
    }
    for (entry = outputs; entry; entry = entry->next) if (same_file(absolute, entry->path)) {
        failure("manifest output aliases another output", absolute);
        free(absolute);
        return 0;
    }
    temporary = malloc(strlen(absolute) + 12);
    if (!temporary) { free(absolute); failure("out of memory", path); return 0; }
    sprintf(temporary, "%s.XXXXXX", absolute);
    fd = mkstemp(temporary);
    fp = fd < 0 ? NULL : fdopen(fd, "wb");
    if (!fp) {
        if (fd >= 0) { close(fd); unlink(temporary); }
        failure("cannot create manifest output", absolute);
        free(absolute); free(temporary);
        return 0;
    }
    fputs("{\"schema\":\"xasm-dependencies\",\"version\":\"1\",\"producer_version\":", fp);
    json_string(fp, producer_version);
    fputs(",\"invocation\":{\"cwd\":", fp);
    json_string(fp, directory);
    fputs(",\"argv\":[", fp);
    for (i = 0; i < argument_count; i++) {
        if (i) fputc(',', fp);
        json_string(fp, arguments[i]);
    }
    fputs("]},\"inputs\":[", fp);
    first = 1;
    for (entry = inputs; entry; entry = entry->next) {
        static const char *roles[] = { "source", "binary", "charmap", "analysis_source", "comparison", "producer" };
        int first_role = 1;
        if (entry->missing) continue;
        if (!first) fputc(',', fp);
        first = 0;
        fputs("{\"path\":", fp); json_string(fp, entry->path);
        fprintf(fp, ",\"size\":%zu,\"sha256\":\"%s\",\"roles\":[", entry->size, entry->hash);
        for (i = 0; i < 6; i++) if (entry->roles & (1u << i)) {
            if (!first_role) fputc(',', fp);
            first_role = 0;
            json_string(fp, roles[i]);
        }
        fputs("]}", fp);
    }
    fputs("],\"missing_paths\":[", fp);
    first = 1;
    for (entry = inputs; entry; entry = entry->next) if (entry->missing) {
        if (!first) fputc(',', fp);
        first = 0;
        json_string(fp, entry->path);
    }
    fputs("]}\n", fp);
    ok = !ferror(fp);
    if (fclose(fp) != 0) ok = 0;
    /* Recheck after writing, before exposing a complete manifest pathname. */
    if (!ok || !dependencies_validate() || rename(temporary, absolute) != 0) {
        unlink(temporary);
        failure("could not publish dependency manifest", absolute);
        ok = 0;
    }
    free(absolute); free(temporary);
    return ok;
}

int dependencies_failed(void)
{
    return (active || protect_outputs) && failed;
}

void dependencies_clear(void)
{
    int i;
    while (inputs) {
        dependency *entry = inputs;
        inputs = entry->next;
        free(entry->path); free(entry->bytes); free(entry);
    }
    while (outputs) {
        dependency *entry = outputs;
        outputs = entry->next;
        free(entry->path); free(entry);
    }
    while (source_paths) {
        dependency *entry = source_paths;
        source_paths = entry->next;
        free(entry->path); free(entry);
    }
    if (arguments) for (i = 0; i < argument_count; i++) free(arguments[i]);
    free(arguments); free(directory); free(manifest_path);
    manifest_path = NULL;
    arguments = NULL; directory = NULL; tail = &inputs;
    argument_count = active = protect_outputs = failed = 0;
}
