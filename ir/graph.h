/* Read-only construction graph views for object serialization. No memo or
 * placement result is exported. Views are borrowed until the unit changes. */
#ifndef XORCYST_IR_GRAPH_H
#define XORCYST_IR_GRAPH_H
#include "extent.h"
#include "heap.h"
ir_unit *ir_unit_create_heap(ir_heap *);
ir_heap *ir_graph_heap(ir_unit *);
typedef enum
{
    IR_GRAPH_INTEGER,
    IR_GRAPH_STRING,
    IR_GRAPH_REFERENCE,
    IR_GRAPH_ADDRESS,
    IR_GRAPH_UNARY,
    IR_GRAPH_BINARY,
    IR_GRAPH_EXTENT_LITERAL,
    IR_GRAPH_COUNT,
    IR_GRAPH_SUM,
    IR_GRAPH_REPEAT,
    IR_GRAPH_REMAINDER,
    IR_GRAPH_BOUND,
    IR_GRAPH_STRING_LENGTH,
    IR_GRAPH_EXTENT_INTEGER
} ir_graph_kind;
typedef struct
{
    ir_graph_kind kind;
    ir_operator op;
    ir_id left, right;
    ir_location location;
    int32_t integer;
    uint64_t extent;
    uint32_t minimum, maximum;
    const unsigned char *bytes;
    size_t length;
} ir_graph_node;
size_t ir_graph_nodes(const ir_unit *);
size_t ir_graph_definitions(const ir_unit *);
size_t ir_graph_anchors(const ir_unit *);
int ir_graph_read(const ir_unit *, ir_id, ir_graph_node *);
int ir_graph_definition(const ir_unit *, ir_id, ir_id *expression, ir_location *);
int ir_graph_anchor(const ir_unit *, ir_id, ir_location *);
/* Internal source-span interning shared by graph and layout storage. */
ir_id ir_graph_location_id(ir_unit *, ir_location);
ir_location ir_graph_location(const ir_unit *, ir_id);
/* Drop the construction-only index. Spans and IDs stay valid; a subsequent
 * constructor rebuilds the index on demand. */
void ir_graph_release_location_index(ir_unit *);
#endif
