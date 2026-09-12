#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "fceux_nl.h"

int fceux_nl_add(fceux_nl_table *table, long bank, int address, char *name)
{
    fceux_nl_entry *grown;
    size_t capacity;
    if (name == NULL) goto fail;
    if (address < 0 || address > 0xFFFF || name[0] == '\0'
        || strpbrk(name, "#\r\n") != NULL) {
        fprintf(stderr, "error: invalid FCEUX .nl name or address: `%s'\n", name);
        free(name);
        return 0;
    }
    if (table->count == table->capacity) {
        capacity = table->capacity == 0 ? 64 : table->capacity * 2;
        if (capacity < table->capacity || capacity > SIZE_MAX / sizeof(*grown)) goto fail;
        grown = realloc(table->entries, capacity * sizeof(*grown));
        if (grown == NULL) goto fail;
        table->entries = grown;
        table->capacity = capacity;
    }
    table->entries[table->count].bank = bank;
    table->entries[table->count].address = address;
    table->entries[table->count++].name = name;
    return 1;
fail:
    free(name);
    fprintf(stderr, "error: out of memory building FCEUX .nl entries\n");
    return 0;
}

void fceux_nl_free(fceux_nl_table *table)
{
    size_t i;
    for (i = 0; i < table->count; i++) free(table->entries[i].name);
    free(table->entries);
    memset(table, 0, sizeof(*table));
}

static int compare_entries(const void *a, const void *b)
{
    const fceux_nl_entry *lhs = a, *rhs = b;
    if (lhs->bank != rhs->bank) return lhs->bank < rhs->bank ? -1 : 1;
    if (lhs->address != rhs->address) return lhs->address < rhs->address ? -1 : 1;
    return strcmp(lhs->name, rhs->name);
}

static void write_entries(FILE *fp, const fceux_nl_table *table, size_t start, size_t end)
{
    size_t i = start;
    while (i < end) {
        size_t j = i + 1;
        int aliases = 0;
        fprintf(fp, "$%04X#%s#", table->entries[i].address, table->entries[i].name);
        while (j < end && table->entries[j].address == table->entries[i].address) {
            if (strcmp(table->entries[j].name, table->entries[j - 1].name) != 0) {
                fprintf(fp, "%s%s", aliases++ ? ", " : "aka ", table->entries[j].name);
            }
            j++;
        }
        fputc('\n', fp);
        i = j;
    }
}

static char *bank_path(const char *prefix, long bank)
{
    size_t length = strlen(prefix);
    char *path;
    if (length > SIZE_MAX - 2 * sizeof(long) - 5) return NULL;
    length += 2 * sizeof(long) + 5;
    path = malloc(length);
    if (path != NULL) snprintf(path, length, "%s%lX.nl", prefix, (unsigned long)bank);
    return path;
}

/* Replace each file only after its complete contents have been written. */
static int write_file(const char *path, const fceux_nl_table *table, size_t start, size_t end)
{
    size_t length = strlen(path);
    char *temporary;
    FILE *fp = NULL;
    int fd = -1, ok = 0;
    temporary = length > SIZE_MAX - 12 ? NULL : malloc(length + 12);
    if (temporary == NULL) goto done;
    snprintf(temporary, length + 12, "%s.XXXXXX", path);
    fd = mkstemp(temporary);
    if (fd < 0) goto done;
    fp = fdopen(fd, "w");
    if (fp == NULL) { close(fd); goto done; }
    write_entries(fp, table, start, end);
    ok = !ferror(fp);
    if (fclose(fp) != 0) ok = 0;
    if (ok && rename(temporary, path) != 0) ok = 0;
done:
    if (!ok) {
        fprintf(stderr, "error: could not write FCEUX .nl file `%s'\n", path);
        if (fd >= 0) unlink(temporary);
    }
    free(temporary);
    return ok;
}

void fceux_nl_free_outputs(fceux_nl_output_plan *plan)
{
    size_t i;
    for (i = 0; i < plan->count; i++) free(plan->destinations[i].path);
    free(plan->destinations);
    memset(plan, 0, sizeof(*plan));
}

int fceux_nl_plan_outputs(fceux_nl_output_plan *plan, const char *rom_prefix,
                          const char *ram_file, long bank_count)
{
    size_t count, i = 0;
    long bank;
    if (rom_prefix == NULL) bank_count = 0;
    if (bank_count < 0 || (unsigned long)bank_count >= SIZE_MAX / sizeof(*plan->destinations)) goto fail;
    count = (size_t)bank_count + (ram_file != NULL);
    if (count == 0) return 1;
    plan->destinations = calloc(count, sizeof(*plan->destinations));
    if (plan->destinations == NULL) goto fail;
    plan->count = count;
    if (ram_file != NULL) {
        plan->destinations[i].bank = FCEUX_NL_RAM_BANK;
        plan->destinations[i].path = malloc(strlen(ram_file) + 1);
        if (plan->destinations[i].path == NULL) goto fail;
        strcpy(plan->destinations[i++].path, ram_file);
    }
    for (bank = 0; bank < bank_count; bank++, i++) {
        plan->destinations[i].bank = bank;
        plan->destinations[i].path = bank_path(rom_prefix, bank);
        if (plan->destinations[i].path == NULL) goto fail;
    }
    return 1;
fail:
    fceux_nl_free_outputs(plan);
    fprintf(stderr, "error: could not plan FCEUX .nl destinations\n");
    return 0;
}

int fceux_nl_write(fceux_nl_table *table, const fceux_nl_output_plan *plan)
{
    size_t i, cursor = 0;
    if (table->count > 1) qsort(table->entries, table->count, sizeof(*table->entries), compare_entries);
    for (i = 0; i < plan->count; i++) {
        const fceux_nl_destination *destination = &plan->destinations[i];
        size_t start = cursor;
        while (cursor < table->count && table->entries[cursor].bank == destination->bank) cursor++;
        if (!write_file(destination->path, table, start, cursor)) return 0;
    }
    return 1;
}
