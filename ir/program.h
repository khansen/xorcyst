/* Object-mode lowering; the value graph and all parser storage stay external. */
#ifndef XORCYST_IR_PROGRAM_H
#define XORCYST_IR_PROGRAM_H
#include "graph.h"
#include "object.h"
typedef struct ir_program ir_program;
ir_program *ir_program_create(ir_unit *);
void ir_program_destroy(ir_program *);
int ir_program_mark(ir_program *, ir_id anchor, ir_location);
int ir_program_raw(ir_program *, const void *, size_t, ir_location);
int ir_program_data(ir_program *, ir_id pc, ir_id expression, unsigned width,
                    const unsigned char *map, ir_location);
int ir_program_instruction(ir_program *, ir_id pc, unsigned char opcode, ir_id expression, int wide,
                           ir_location);
int ir_program_storage(ir_program *, ir_id pc, ir_id extent, ir_location);
int ir_program_ram_label(ir_program *, ir_id anchor, unsigned alignment, int zeropage, ir_location);
int ir_program_ram_storage(ir_program *, ir_id pc, ir_id extent, ir_location);
int ir_program_export(ir_program *, const char *name, ir_id expression, ir_location);
int ir_program_require(ir_program *, ir_id expression, ir_location);
ir_id ir_program_bank(ir_program *, ir_id expression, ir_location);
/* Produces a fully validated independent object. The program is consumed for
 * construction purposes, but remains destroyable. Caller owns the object. */
ir_object *ir_program_object(ir_program *);
const char *ir_program_error(const ir_program *);
#endif
