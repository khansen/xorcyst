/* Object-mode lowering; the value graph and all parser storage stay external. */
#ifndef XORCYST_V2_PROGRAM_H
#define XORCYST_V2_PROGRAM_H
#include "graph.h"
#include "object.h"
typedef struct v2_program v2_program;
v2_program *v2_program_create(v2_unit *);
void v2_program_destroy(v2_program *);
int v2_program_mark(v2_program *, v2_id anchor, v2_location);
int v2_program_raw(v2_program *, const void *, size_t, v2_location);
int v2_program_data(v2_program *, v2_id pc, v2_id expression, unsigned width,
                    const unsigned char *map, v2_location);
int v2_program_instruction(v2_program *, v2_id pc, unsigned char opcode, v2_id expression, int wide,
                           v2_location);
int v2_program_storage(v2_program *, v2_id pc, v2_id extent, v2_location);
int v2_program_ram_label(v2_program *, v2_id anchor, unsigned alignment, int zeropage, v2_location);
int v2_program_ram_storage(v2_program *, v2_id pc, v2_id extent, v2_location);
int v2_program_export(v2_program *, const char *name, v2_id expression, v2_location);
int v2_program_require(v2_program *, v2_id expression, v2_location);
v2_id v2_program_bank(v2_program *, v2_id expression, v2_location);
/* Produces a fully validated independent object. The program is consumed for
 * construction purposes, but remains destroyable. Caller owns the object. */
v2_object *v2_program_object(v2_program *);
const char *v2_program_error(const v2_program *);
#endif
