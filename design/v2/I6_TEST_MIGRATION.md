# I6 test migration

The installed `xasm` and `xlnk` now run the reviewed shared backend. The direct
suites are renamed from `test_v2_*` to `test_ir_*`; their C/Python oracles,
allocation sweeps and randomized cache/layout comparisons are retained.
`test_v2_analysis.py`'s wrapper is removed because the five analysis suites now
exercise the normal tool directly. Existing source cases still exercise binary
and object/link paths; analysis requests are made only in binary mode.

## Retired representation tests and replacements

These changes retire assumptions about the old wire representation, not the
source-level guarantees. Current codec tests reject bad structure before any
translation or publication.

| Old control in `test_inputs.py` | Current coverage |
| --- | --- |
| Segment terminators, trailing bytes, every truncated prefix | `test_ir_objects.py`: `test_header_boundaries_padding_and_reserved_fields`, `test_invalid_references_opcodes_maps_and_fixup_ownership`; the codec failure harness tests truncations and exact sizes. |
| Bytecode IDs, bad references, recursive expression-depth field | The same codec controls plus `test_cycles_are_valid_structure_but_fail_if_required` and `test_large_shared_graph_is_not_expanded_into_a_tree`. Expressions are table graphs, not recursive bytecode. |
| 16,384-expression ceiling | `test_objects_exceed_the_retired_expression_limit_and_share_duplicates` assembles and links beyond that limit, with exact bytes. |
| Compact signed integer widths and object metadata helpers | `test_all_expression_tags_and_counted_strings`, signed relocatable/exported arithmetic controls in `test_inputs.py`, and the 244-byte approved wire fixture. All wire fields now have fixed widths. |
| Instruction operand types, short-address boundaries, negative immediates and backward branches | `test_ir_layout.py`: `test_byte_operands_and_zero_page_boundaries`, `test_word_operand_mode_policy_and_data_warnings`, `test_branch_edges`, `test_every_documented_6502_opcode_encodes_its_fixed_operand_width`; source/object controls in `test_ir_objects.py`. |
| Explicit-address labels | Approved located rejection is pinned by `test_ir_frontend.py`, including macro/repeat expansion. The general directive fixture now uses ordinary `.label`. |
| Storage count bounds and eager rejection of forward counts | `test_checked_extents_do_not_wrap_or_validate_absent_elements`, `test_limits_and_fixed_width_selection`, and source field-initializer/count tests. Forward counts may serialize and fail at link; tests preserve the prior linked output on failure. |
| Old linker hash-table/object allocation wrappers | `test_allocation_failures_release_every_owned_object`, heap mapping failure sweeps, layout allocation sweep and CLI output-failure tests. `test_unit_alloc.c` and `test_xlnk_io_faults.c` referenced retired modules and are removed. |

`test_linker_outputs.py` retains independent CLI checks for copy, aliases, size
limits and staging failures. The shell RAM alignment fixture now checks the
encoded `small_var` address directly; verbose progress is checked separately.
Old malformed bytecode fixtures are replaced by an actionable rebuild rejection
for old objects; current malformed records are exercised by the codec suite.

## Source, analysis and CLI controls

- The instruction coverage fixture now has an in-range branch target, so it can
  validate both object emission and binary analysis. Dedicated branch-boundary
  tests still cover rejection and preservation of previous output.
- Old source-time `$` snapshots after unresolved shortening now assert the
  approved located rejection. Explicit-address `.label`, type-body `$`,
  exported aliases, imported strings and delayed counts follow the reviewed
  contract. Generic shared-backend diagnostic wording replaces old text where
  the error remains located and publication still fails closed.
- Diagnostic listings retain source/opcodes and provisional extents, never
  claim final emitted bytes. Tests assert populated rows, not a vacuous loop.
- Object-mode xref/listing tests assert rejection and preservation of the old
  object and sidecars. Object `--compare` remains supported, including CPU-base
  fallback; it is a byte comparison, not a structured analysis export.
- The old schema-1 selector/default controls are replaced by
  `test_default_schema_and_removed_selector`: default schema 2, populated rows,
  rejection of selectors 1/2/invalid and preservation of previous outputs.
- Existing expression tests exposed captured aliases that bypassed the AST's
  undeclared-name walk. The normal frontend now warns for implicit external
  aliases and rejects missing `@@` locals (including later other-scope labels).
- Installation tests check exactly the normal tool versions, shipped migration
  guide/manual/man pages and an actual installed object/link/binary round trip.

The old optional constant-cache allocation hook is removed from the AST work
harness; it did not exercise the active shared evaluator. Cache ownership,
allocation failure, mixed-mode invalidation and rollover remain covered by the
IR tests. Backend work-count tests still instrument source expansion and JSON
analysis independently of wall-clock timing.
