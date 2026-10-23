# I1: shared values and owned dependencies

This module implements the value/ownership stage of the approved
[D1 contracts](../XORCYST_V2_CONTRACTS.md), sections 1 and 2. It is built by
the test harness only. Installed `xasm` and `xlnk` still use v1; their source
lists do not include this module. I1 does not introduce a parser, placement
algorithm, object reader/writer, analysis renderer or command-line option.

## Ownership and binding

`value.h` is the internal interface. One `v2_unit` owns names, locations,
counted string payloads, definitions, anchors and expression nodes. IDs are
unit-local and remain valid when tables grow. A location contains an owned
file ID and a copied source span. Name and string constructors copy the caller's
bytes. Destroying a partially built unit is safe, including after allocation
failure. The module contains no borrowed parser AST pointers.

The frontend will select definitions using the existing binding rules. A
definition starts unresolved and can be bound once. Captured assignments with
the same spelling receive distinct IDs; a pending forward use can be bound to
the frontend's eventual selection. The module never looks up a name by spelling.
This represents the approved capture rules; integration tests of source scopes,
`.undef`, reassignment and import/export binding remain frontend/linker work.

Anchors are unresolved until `v2_place` supplies an integer value. They are
dependency inputs, not storage fragments or an address-allocation policy.
Layout, bank lookup and cross-unit import resolution will supply facts in later
stages. An absolute value already known independently of placement is an integer
node. Node IDs and operator enums are internal and are not the wire encoding.

## Evaluation

`v2_evaluate` returns Known, Deferred or Invalid. Deferred results expose unique
unresolved definition/anchor IDs; Invalid results carry a diagnostic code and
source location. The caller reports diagnostics and enforces the final-output
gate. Evaluating an unrelated valid root still works after a semantic error;
an allocation failure or invalid builder operation instead poisons the unit.

Evaluation uses an explicit stack and preserves shared graph edges. Active
nodes detect cycles, completed nodes are memoized. Binding and placement changes
invalidate dependent results, including cached errors. Constant results survive
unrelated changes. Dependencies are collected once for a requested Deferred root,
rather than copied into every ancestor. Evaluation visits operands left to right;
the first Invalid operand wins, while a Deferred left operand does not hide an
Invalid right operand. Logical operators evaluate both operands.

All integer operations use the existing `expr_int.h` semantics. Strings preserve
their lengths and embedded zeros. `+` supports concatenation and signed-decimal
integer conversion; comparisons require two strings and compare unsigned bytes
lexicographically. Unsupported operand kinds are Invalid. No `x+0` simplification
discards an unresolved value's eventual string kind. A string operation requiring
placement is Invalid once its operands resolve; before that it remains Deferred.

Strings are limited to 16 MiB each and 256 MiB of live unique payload per unit.
Aliases share payloads. Constant concatenations allocate once and remain owned
until unit destruction. Returned strings and names remain valid until then;
the dependency array is borrowed until the next evaluation. The unit is not
thread-safe. Table growth is checked and there is no 16,384-expression limit.

## Validation and remaining gates

Run `python3 tests/test_v2_values.py`. `make check` and `make distcheck` include it.
`TEST_V2_CFLAGS` selects instrumentation; CI runs it under ASan/UBSan on Linux.
Allocation injection and epoch-rollover access exist only in the test wrapper,
not in the module. The tests cover ownership, captured IDs, deferred bindings,
cycle diagnostics, cache invalidation, counted strings, resource limits and
failure during every allocation in a representative growing graph. A Python
integer oracle checks all operators through literal, binding and anchor paths.
A 100,000-level chain and a shared 200-level diamond exercise stack safety and
memoization without relying on a performance threshold.

I1 has no end-to-end v2 assembler or linker. The required ROM equality and
project performance gates therefore remain open for the later stages. Passing
these unit tests does not certify those gates or authorize cutover.
