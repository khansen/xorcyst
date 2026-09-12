#ifndef XASM_FCEUX_NL_H
#define XASM_FCEUX_NL_H

#include <stddef.h>

#define FCEUX_NL_PAGE_SIZE 0x4000
#define FCEUX_NL_RAM_BANK (-1L)

/* Output records, not assembler symbols. Names are owned by the table. */
typedef struct {
    long bank;
    int address;
    char *name;
} fceux_nl_entry;

typedef struct {
    fceux_nl_entry *entries;
    size_t count;
    size_t capacity;
} fceux_nl_table;

typedef struct {
    long bank;
    char *path;
} fceux_nl_destination;

/* Exact destinations, including RAM, in the same bank order as the entries. */
typedef struct {
    fceux_nl_destination *destinations;
    size_t count;
} fceux_nl_output_plan;

/* Takes ownership of name, including on failure. NULL means allocation failed. */
int fceux_nl_add(fceux_nl_table *table, long bank, int address, char *name);
void fceux_nl_free(fceux_nl_table *table);
/* Planning fills a zero-initialized plan; it neither registers nor opens files. */
int fceux_nl_plan_outputs(fceux_nl_output_plan *plan, const char *rom_prefix,
                          const char *ram_file, long bank_count);
void fceux_nl_free_outputs(fceux_nl_output_plan *plan);
/* The caller validates the complete invocation's destinations before writing. */
int fceux_nl_write(fceux_nl_table *table, const fceux_nl_output_plan *plan);

#endif
