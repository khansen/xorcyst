#include "dependencies.h"
#include "sha256.h"
#include <stdlib.h>
#include <string.h>

static int check_protected_alias(const char *kind, const char *first, const char *second)
{
    FILE *fp;
    int ok = dependencies_start_output_protection();
    if (ok && strcmp(kind, "source") == 0) {
        ok = dependencies_protect_source(first);
    } else if (ok && strcmp(kind, "input") == 0) {
        fp = dependencies_open(first, "rb", DEP_SOURCE);
        if (fp != NULL) fclose(fp);
        ok = !dependencies_failed();
    } else if (ok) {
        ok = dependencies_output(first);
    }
    if (ok && strcmp(kind, "late-input") == 0) {
        fp = dependencies_open(second, "rb", DEP_SOURCE);
        if (fp != NULL) fclose(fp);
        ok = !dependencies_failed();
    } else if (ok) {
        ok = dependencies_output(second);
    }
    dependencies_clear();
    return ok ? 0 : 3;
}

int main(int argc, char **argv)
{
    FILE *fp;
    unsigned char buffer[16384];
    char hash[65];
    size_t count, chunk = sizeof(buffer);
    sha256_context ctx;
    int hash_only = argc >= 3 && strcmp(argv[1], "hash") == 0;
    int protect_only = argc >= 3 && strcmp(argv[1], "protect") == 0;
    if (argc == 5 && strcmp(argv[1], "protect-alias") == 0)
        return check_protected_alias(argv[2], argv[3], argv[4]);
    if (argc < 3 || (!hash_only && argc != 4)) return 2;
    if (hash_only) {
        fp = fopen(argv[2], "rb");
        if (argc > 3) chunk = (size_t)strtoul(argv[3], NULL, 10);
        if (!fp || !chunk || chunk > sizeof(buffer)) return 2;
    } else {
        dependencies_arguments(argc, argv);
        if (!(protect_only ? dependencies_start_output_protection()
                           : dependencies_start("dependency-test-driver", argv[1]))) return 3;
        fp = dependencies_open(argv[3], "rb", (unsigned)strtoul(argv[2], NULL, 10));
        if (dependencies_failed()) { dependencies_clear(); return 3; }
        puts("ready");
        fflush(stdout);
        if (getchar() == EOF) { dependencies_clear(); return 2; }
    }
    sha256_init(&ctx);
    if (fp) {
        while ((count = fread(buffer, 1, chunk, fp)) != 0) sha256_update(&ctx, buffer, count);
        if (ferror(fp) || fclose(fp) != 0) return 3;
        sha256_finish(&ctx, hash);
        printf("snapshot=%s\n", hash);
    }
    if (hash_only) return 0;
    count = protect_only ? !dependencies_failed() : dependencies_write(argv[1]);
    dependencies_clear();
    return count ? 0 : 3;
}
