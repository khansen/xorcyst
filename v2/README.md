# I1: shared values and owned dependencies

This module implements the value/ownership stage of the approved
[D1 contracts](../XORCYST_V2_CONTRACTS.md), sections 1 and 2. Initially built only by
the test harness, it is also used by the isolated I3 target below. Installed `xasm` and `xlnk` still use v1; their source
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

## I3: isolated source frontend

`make xasm-v2` builds a non-installed prototype. Invoke it with
`xasm-v2 --pure-binary input.asm -o output.bin`. It accepts the existing source
syntax, includes, macros, conditionals, command-line definitions and debug
option. Object output, listings, analysis, debugger exports, comparison and
manifests are rejected explicitly in this stage. Installed `xasm`/`xlnk` remain
v1. The prototype has no object reader and cannot consume old objects.

`frontend.c` compiles the existing parser/expansion implementation with
`XORCYST_V2_FRONTEND`; the normal target does not define that macro. The adapter
reuses structural lowering and source binding, then builds the owned value
and layout units. Constant captures use shared definition IDs, including
forward aliases, rather than recursively copying alias trees. Assignment and
`.undef` generations retain the approved v1 rules. An alias waiting for a type
or member declaration retains an owned structural expression until it can be
lowered; later reassignment cannot change that captured expression. Character
maps are copied at each `.char` use. Literal data is coalesced between anchors, while dynamic
operands keep their own fixups and `$` anchors.

Label/procedure/variable declarations retain their own source identity when
`.undef` removes their symbol. Uses still resolve by v1's final-name rules;
retired declarations keep their emitted data without placing the replacement's
anchor. AST clones preserve only the explicit-address rejection marker, not
transient macro-local binding flags.

Source-time size decisions recheck those bindings too. `sizeof(label)` keeps a
kind and string-length guard for nonliteral data operands, including currently
defined labels and forward names that are known at the sizeof use. Each guard
records the contribution counted by that scan. An owned pending list, addressed
by a handle on the native statement, binds guards by walking the live tree at
the end of pass 1. This captures both the data-line binding and the completed
renaming of anonymous `+`/`-` references, even when the statement had already
been reduced at the sizeof use. No guard retains a statement pointer or assumes
it survives pass 1. This includes operands following sizeof in the same statement. A later
reassignment changing its kind or string length rejects at the original use;
numeric changes and same-length strings remain valid.
A source-time `$` use records which preceding data operands were assumed to
remain numeric. After final-name binding, a replacement string invalidates
that assumption at the original size/PC use. PC guards are segment-local;
an absolute origin starts a new prefix without discarding earlier uses' checks.
Their borrowed operands are checked and released before second-pass rewriting.
Data operands use the same local-label namespace as the ordinary first-pass
walk, resolved before testing widths, so backward `@@` labels do not falsely
defer a following source-time PC. End-of-pass guard capture also sees the renamed
`@@` operands following sizeof on its own statement. Forward local names still
follow the conservative unknown-width rule for source-time PC uses.

A scan reaching an unprocessed user-typed initializer cannot bind that guard:
pass 1 replaces and frees the initializer while flattening it. The unbound
guard gives a located size-unknown error at the sizeof use, preserving the
earlier conservative rejection. An aggregate processed before the scan remains
supported. Pending handles are not inherited by AST clones. The capture walk
runs only when a native statement has pending guards.

During a label-size scan, another operand containing `sizeof` of that same
data run contributes one element without recursively evaluating the size again.
Leading labels separated only by internal markers share that run; their scans
have the same first data/storage node. Unrelated label sizes still reduce
normally, including when used to construct strings. This comparison is local
to the scan and retains no new AST pointers after it returns.
The ordinary data pass still evaluates each use at its own source position.
The entire operand retains a numeric-kind guard, including the scan's own
operand: concatenating a string with `sizeof` must not silently enlarge it.
Storage counts and type dimensions still use the existing cycle checks.
The owner explicitly approved rejection of self-sized strings on 2026-10-24;
the contract ledger and NEWS record the v1 compatibility change separately
from the reassignment rule.

The plain prototype compacts fully literal data after source-time size checks,
releases lowered operands immediately, stores immutable instruction integers
directly in their fixups, and inspects anchors without allocating temporary
value nodes. Its AST stores the diagnostic span once. Original analysis
provenance must be captured separately before I5 enables renderers; this stage
does not claim analysis support or waive that stage's memory/performance gates.

Every invocation destroys the parser AST and symbol table before calling v2
layout and emission. Final bytes come only from the validated I2 snapshot.
The prototype retains atomic output staging and protects all opened source,
binary-include and charmap inputs from output aliases. It does not read source
locations again to render analysis, so it needs no diagnostic-reader path walk.

The source controls cover the approved changes: fixed-point instruction
narrowing; forward strings and use-site character maps; integer-kind checks
before identities; acyclic late storage spans; rejection of provisional `$`
in expansion, type-body `$`, and explicit-address `.label`; and assembler-time
`sizeof(label)` with local forward-kind validation. The 50 reviewed D1 sizeof
cases are included as source fixtures, alongside macro-exit, macro-label and
redundant-segment boundaries. No new source operators are added.

Project profiling led to representation optimizations in I2: fixed
instruction and integer-data extents share synthetic width literals and avoid
unnecessary mutable nodes; fixed
stretches of a prefix accumulate as checked offsets; and prefix/span graphs
are built only when checked extents need them. Span lowering visits only the
graph reachable from extent roots. Ordinary operands still use placed addresses
in walk order. Direct-IR controls, cache differentials, allocation injection
and sanitizer tests apply to these changes as well as the new source suite.

Run `python3 tests/test_v2_frontend.py` or `make check`. The new suite honors
`TEST_V2_CFLAGS` and runs in the Linux sanitizer CI job after publication.
`tests/perf_v2_frontend.py` implements the plain Metroid/Zelda paired timing
protocol using separately built release executables and the frozen D1 inputs.
It checks every output and diagnostic stream, records raw timing/RSS samples,
and reports the prescribed bootstrap intervals. Timing runs are outside
`make check`; use an idle machine and retain each session independently.

The adapter currently uses private AST bookkeeping fields while analysis is
explicitly disabled. I5 must capture source provenance independently before
enabling those output modes; the prototype does not claim analysis support.

I3 completed local independent review and received owner approval; see below.
Object/link ROM gates (including dpadhero2), listing/analysis migration and their
performance configurations belong to I4/I5. They are not waived by the plain
prototype, and the installed-tool cutover remains I6.

## I3 approval record

Kent Hansen approved I3 on 2026-10-24 after independent local review reported
no findings or blockers on `dc45658181a755175658bb60ce3c1b38da1dfec2`. Its runtime
is identical to the independently cleared `7ffb2ac`; the final follow-up adds
the `.exitm` tombstone regression control and updates the validation record.
The owner authorized publication as the single I3 PR and its CI run.

This approval covers the isolated direct-binary source frontend, its reviewed
representation changes, tests and recorded compatibility decisions. It does
not waive PR CI, later reviews, required object/link ROM gates (including
dpadhero2), analysis/bundle performance or final cutover. Installed v1 tools
remain unchanged. This approval record adds no executable change; the PR
retains the reviewed revision and this documentation commit.
