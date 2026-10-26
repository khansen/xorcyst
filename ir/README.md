# Shared implementation

`xasm` lowers expanded source into an owned expression graph and layout IR.
`xlnk` reads XOROBJ2 objects into the same representation. Both use `value.c`
for values and dependencies, `layout.c` for placement and instruction narrowing,
and the same encoder for final bytes. There is one installed assembler and linker.

- `frontend.c` owns source binding, captures, source-time checks and analysis
  metadata. It includes `astproc.c` to keep private expansion helpers in one
  translation unit. Parse statements are released as they are lowered.
- `value.c`, `graph.h` and `extent.h` implement Known/Deferred/Invalid values,
  checked layout extents, memoization and dependency invalidation.
- `program.c` writes the portable object tables. `object.c` checks the complete
  wire input before translation. `link.c` resolves providers and imports.
- `heap.c` pools small linker allocations. Its tradeoffs, including sanitizer
  limits and measured transient object-buffer costs, are in the performance
  protocol and historical I4 validation record.
- `path.h` implements future-path identity checks used by both drivers.

Integer arithmetic wraps to signed 32-bit values through `expr_int.h`.
Strings are limited to 16 MiB each and 256 MiB of unique live payload per unit.
Known string bytes live until unit destruction; empty strings may have a NULL
pointer and must not be passed to memory/string functions even with length zero.
Extent evaluation may use lowered same-run spans; ordinary instruction operands
use placed addresses and retain the specified in-place narrowing order.

The graph and layout modules have no source-parser dependencies. Direct-IR tests
exercise them independently; frontend, object and analysis tests use the normal
command-line programs. Run `make check`; `TEST_IR_CFLAGS` selects instrumentation
for the direct-IR and source/object integration suites.

The semantic contract, wire specification and recorded owner decisions remain in
`XORCYST_V2_CONTRACTS.md` and `design/v2/`. Historical stage approvals are retained
in `design/v2/IMPLEMENTATION_HISTORY.md`; they do not certify the final cutover.
