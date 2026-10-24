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
Invalid right operand. Binary logical AND/OR are excluded: they have no v1
syntax or demonstrated project use (D1 erratum in contract section 9).

All integer operations use the existing `expr_int.h` semantics. Strings preserve
their lengths and embedded zeros. `+` supports concatenation and signed-decimal
integer conversion; comparisons require two strings and compare unsigned bytes
lexicographically. Unsupported operand kinds are Invalid. No `x+0` simplification
discards an unresolved value's eventual string kind. A string operation requiring
placement is Invalid once its operands resolve; before that it remains Deferred.

Strings are limited to 16 MiB each and 256 MiB of live unique payload per unit.
Aliases share payloads. Constant concatenations allocate once and remain owned
until unit destruction. Empty strings may have a NULL byte pointer; callers
must skip byte access and memory/string calls when the length is zero.
Returned strings and names remain valid until unit destruction;
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

Rollover tests retain cached epoch-1 results and dependency visit marks before
wrapping each counter independently. Separate tests cover first-error selection
after placement or binding a dependency in a cycle, and the total live string
budget on concatenation as well as literal construction.

I1 has no end-to-end v2 assembler or linker. The required ROM equality and
project performance gates therefore remain open for the later stages. Passing
these unit tests does not certify those gates or authorize cutover.
The I1 baseline used 168 bytes per node on the reviewed 64-bit host and
invalidated all placement-dependent caches at each placement epoch change.
I2 must measure those costs and repeated deferred-dependency walks.
Complete local independent review before opening or updating an implementation PR.

## I1 approval record

Kent Hansen approved I1 on 2026-10-23 after independent local review reported
no blockers on `f21cde93cd94b1d9200f0c061ad1d4ab9e3fc954`. That revision includes
the owner-approved D1 operator erratum and the reviewed coverage/API follow-up.
The owner authorized publication as the single I1 PR and its CI run.

This approval covers the isolated value graph and its tests. It does not waive
PR CI, the later implementation reviews, required project ROM/performance gates
or final cutover. The installed v1 tools remain unchanged. The PR records its
published head and eventual merge commit; this approval record adds no executable
change to the reviewed revision.

## I2: isolated layout and encoding

`layout.h` consumes directly constructed IR; `extent.h` adds checked uint64
layout quantities to the shared evaluator. They are internal operations, not
new source-language operators or wire tags. Counts, sums, repeated extents,
field remainders, bounds and counted string lengths keep their checks before
coalescing. Zero repetition skips an absent inner element. An explicit extent
to integer conversion projects the low 32 bits only after checking the extent.

One layout borrows a live value unit, captures raw/copy bytes and character maps,
and runs once. Bind definitions before running. Each statement/operand has its
own PC anchor. Source lowering must supply captured identities, checked storage
constraints, RAM source spans and early-truncation flags; it must complete
`sizeof(label)` itself. I2 does not implement that frontend or accept source
files. Code-anchor differences in one contiguous run use shared intervening
extent sums only for extent queries. Ordinary operands keep their original
subtraction of placed addresses, including previous-walk forward values. Bound
aliases are followed; arbitrary algebra/equation solving is not added. Origin
changes, banks and dynamic padding separate runs.

Link RAM objects arrive in unit order and v1 emitted-label order. The exact
last-pivot quicksort permutation is retained before first-fit allocation;
alignment holes stay usable, including splits made before discovering that an
aligned remainder cannot fit. Zero-size labels cannot use a block's exclusive
end. Binary RAM uses source-order cursor markers,
including data origins and earlier code/RAM dependencies. Its alignment and
zero-page annotations do not introduce padding or placement constraints, as
in v1. Code constants and available RAM values select widths before placement;
every subsequent source-order walk can irreversibly narrow an instruction.
Walks stop when nothing shortens, followed by final operand/extent validation.
The approved seven narrowing controls and a 200-candidate cascade are tested.

Final snapshots carry fragment kind, location, address, output offset/length,
bank, final opcode, operand value and warning status. Their extents cover every
output byte, including automatic bank padding. Encoding uses these validated
facts and captured values. Failed construction/layout/validation/copy checking
exposes neither bytes nor facts. Captured copy files are compared byte-for-byte
before the snapshot becomes available. The module never opens an output file;
filesystem staging/rename and analysis renderers remain integration work.

Checked prefix expressions supply code and source-RAM addresses only during
extent evaluation. Ordinary operands still use currently placed anchors; future
addresses are not made available early to the narrowing loop. This lets an
acyclic forward count resolve through its dependencies, and reports a real
self-dependent count as a cycle. The two query modes have separate memo entries
and dependency closures; a query cannot overwrite the other mode's descendants.
Cycle diagnostics are reused only within the current request, so the first
repeated edge does not depend on earlier queries. Finished concatenations own
one immutable buffer until unit destruction, even if a cache is recomputed.
Final validation checks extents against the facts already used for placement.

The evaluator now invalidates cached ancestors through reverse edges when an
anchor changes, preserving unrelated cached results. Extent nodes and ordinary
expressions share the iterative evaluator and cycle detection. Span rewriting
is a construction-time lowering step; it clears dependent caches and retains
pure completed values and shared graph nodes. Bindings remain immutable once
selected. Work counters
report evaluations, invalidation edges and deferred-dependency walks. See the
owner-approved contract section 10: repeated shared dependencies can require
superlinear work within a walk. Tests bound that adversarial case and check
that unrelated placements do not re-evaluate a shared operand at every use.

Run `python3 tests/test_v2_layout.py`. Its harness uses the real modules with
allocation wrappers, as I1 does. `make check`, distribution tests and sanitizer
CI run both v2 suites. Installed v1 source lists still exclude both modules.
No project ROM equality, end-to-end v2 performance, source binding, object
serialization/linker integration or downstream analysis gate is claimed here.
I2 completed local independent review before publication; see its approval below.

On the local arm64 build the I2 node is 288 bytes (up from the initially reviewed
192, to keep the two memo closures independent), a reverse edge 8 bytes,
a fragment 136 bytes and a final fact 56 bytes. Character maps allocate only
when used. A 1,000/5,000-instruction shared-operand fixture evaluates 2,805/10,805
nodes in two walks. The adversarial 100/200-anchor fixture evaluates
10,899/41,799 nodes in three walks; this is recorded work, not a project timing
claim. The 200-step cascade takes 202 walks and 40,602 fragment visits.

## I2 approval record

Kent Hansen approved I2 on 2026-10-24 after independent local review reported
no findings on `35fbb0cd8c9d56f0596028fb10bbe22d504e51fa`. That revision contains
the reviewed layout/encoding implementation, the owner-approved work-bound
erratum, and the coverage follow-up for span lowering, epoch rollovers and
zero-page failed alignment splits. The owner authorized publication as the
single I2 PR and its CI run.

This approval covers the isolated layout/encoding module and its tests. It does
not waive PR CI, later implementation reviews, required project ROM/performance
gates or final cutover. Installed v1 tools remain unchanged. This approval record
adds no executable change to the reviewed revision; the PR retains that revision
and this documentation commit. I3 remains the next implementation stage after
I2's PR workflow is complete.
