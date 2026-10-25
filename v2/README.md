# I5 local review candidate: analysis integration

I4 merged in PR #100 (`63747ed`) on 2026-10-25 after local review, owner
approval and green CI. Independent local review cleared I5 at `63e4ea5` /
Nesrev `4ca95e06d` with no blockers. Owner approval remains pending; nothing
has been published. Installed tools still use the existing implementation.

The first I5 change prepares source capture. Frontend string/map cache IDs and
analysis provenance IDs now occupy distinct fields. Optional original source
locations are stored in the AST node's allocation, after the fixed node, and
read through `astnode_source_location()`. Clones retain the original source
span separately from their expansion location. The capture policy is selected
before parsing; the presence of the extra storage is recorded on each node.
Plain assembly does not allocate that storage. On the current 64-bit host the
fixed node remains 136 bytes, as in I4; enabling capture adds a 32-byte location
without a second allocation. This is source metadata, not another evaluator.

The data-provenance hook runs before the v2 data handler reduces operands.
Lowering associates source rows with shared-layout fragments. Native statement
metadata for rich collectors transfers out of the source tree; text/structured
listings use lightweight rows without rebuilding those expressions. Declarations and symbol definitions
are copied into an owned analysis index. File names are interned in the value
unit. No collector borrows the parser namespace, which is destroyed before
layout. The final view supplies addresses, opcodes, extents and encoded bytes;
operand/term queries use the same value graph as emission.

Use `./xasm-v2 --pure-binary input.asm -o output.bin --listing=listing.json
--listing-format=json`; NDJSON and text are also supported. Schema 2 is the
only structured listing schema. Xref, standalone/embedded instruction records,
summary, index patterns, data consumers, coverage, comparison, raw-address
audit, dependency manifests and FCEUX exports are enabled. Structured analysis
remains pure-binary-only. The existing safe diagnostic-listing exception has
no emitted-byte claims and still requires complete output-destination planning.

The Nesrev migration uses an isolated local checkout and the same frozen source
inputs as the baseline. The source checkout's `design/v2/I5_WORK.md` records
implementation and remaining gates. `design/v2/I5_REVIEW.md` is the reviewer
prompt; `I5_VALIDATION.md` and `i5-current.json` record the checks and measured
costs, including the owner's bounded-effort decision. Installed tools stay unchanged;
local independent review and owner approval precede an I5 PR. The prototype
names and build selection are temporary and are removed at the I6 cutover.

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

At I2 approval on the local arm64 build the node was 288 bytes (up from the initially reviewed
192, to keep the two memo closures independent), a reverse edge 8 bytes,
a fragment was 136 bytes and a final fact 56 bytes. I4 storage changes are
described below; those historical sizes are not current allocation estimates. Character maps allocate only
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


## I4: object and linker integration (approved for publication)

Build `make xasm-v2 xlnk-v2`. The source frontend now writes XOROBJ2 objects
unless `--pure-binary` is selected. `xlnk-v2` reads those objects and the existing
linker-script syntax (`ram`, `link`, `copy`, `bank`, `pad`, and `output`). It uses
the same evaluator and layout engine as direct-binary assembly. These are
non-installed check targets. Listings/analysis remain disabled; no v1 object
reader or format autodetection fallback is provided.

Numeric script arguments deliberately require the entire token to be valid.
Unlike v1's prefix parsing, `8000h`, `$C000x`, `1e3` and bare `$` reject with a
located argument error and keep the previous output. The owner approved this
correction on 2026-10-25; valid numeric spellings and script commands are unchanged.

`object.h` owns fixed-width tables and counted blobs. Decode validates the
complete envelope, lengths, table references, field usage, fixup ownership,
opcodes, character maps and reserved tags before linking. Integers are read
and written explicitly in little-endian order. Cyclic graphs are structurally
valid; a required cyclic value fails during evaluation. The approved 244-byte
example round-trips byte-for-byte. Objects contain only definitions and the
reachable shared expression/extent graphs, never evaluator caches or AST pointers.
The writer omits redundant spans on definition-reference and address leaves,
which the linker locates through their owners, and shares equivalent leaves
using small construction-only ID maps. It retains literal/operator/bank spans,
RAM-position wrapper locations and per-operand fixup spans. Cycle proxies also
fall back to owner locations for spanless leaves. I5 consumes direct-binary
facts; it does not require term spans from objects. No wire-format change or
general graph-normalization pass is involved.

The source adapter keeps declaration-time captures and final-name binding from
I3. A reassignment invalidates cached captures after its RHS has used the old
definition and the new definition is installed. This preserves each captured
step of `S = Ext / S = S+1` even while Ext is forward or imported.
Exports are captured after public tagging but before unused-label removal, so
an exported alias can be a private label's only reference. Other unused labels
are still removed with v1's warning policy. When unused-equate checking is
enabled, constant reference counts are saved and restored around early export
capture: an export is not a source use for W0201. Label dependencies stay live.
Leading unnamed RAM storage has no allocation identity and is discarded even
when its count uses `$`; an ordinary count needs no extra PC anchor.
Storage after a surviving label still extends that label's RAM allocation.
Writer errors without a source span report `location unavailable`, never a
NULL filename. Deferred initializer lengths use extent tag 8: one element for an integer,
counted byte length for a string (owner clarification, 2026-10-25). Native
field remainders therefore work with imported numbers, strings, empty strings,
word strings, nested fields and unions. Imported kinds come from the provider;
`.extrn` annotations remain local typing hints. Deferred `.char` values carry
the map captured at their use. Existing `sizeof(label)` kind/length guards run
before an object can be published. Fixed instructions and raw data coalesce
without losing address anchors or branch/range checks.

The linker consumes one object at a time. Each append copies the retained
values, bytes and spans into the native unit, then discards the decoded tables
and reuses one translation workspace for the next object. Decoded blob
metadata stays in the checked wire storage until an edit materializes it.
After the last append it resolves exports, imports
and bank requests. Binding tables are discarded before layout. RAM entries
retain unit order and emitted-label order, the exact v1 quicksort permutation,
and v1's first-fit allocator including failed alignment splits. Initialized
RAM data follows v1 linked behavior: it contributes no allocation, but its
referenced imports must still bind. The approved binary/link differences remain
in force, including the linked 64K code limit and late wide-operand errors.
Without script RAM blocks, sizes are still checked but placement is skipped,
as in v1. Unused or unused-exported RAM therefore does not prevent a link;
actual address uses still fail because their anchors remain unresolved. This
does not skip validation when supplied blocks are exhausted during allocation.

The shared storage now uses fixed-size slabs for nodes, memo entries,
locations and layout fragments. IDs remain stable as directories grow.
Common spans occupy eight bytes with an owned full-width fallback; no location
field is truncated. The two query modes still have separate lazy memo slots,
reverse edges and rollover handling. Temporary traversal marks are cleared
after each walk; cache validity still uses binding/request epochs. Nodes occupy 32 bytes; lazy memo records
occupy 16 bytes. Known values share their payload storage with the epoch used
only by Deferred/Invalid states. A cycle uses its request epoch; other
binding-dependent results use the binding epoch. Known results cannot depend
on a definition that remains unbound. Immutable strings retain their original
lifetime. Layout fragments occupy 28 bytes, with 16-byte auxiliary records.
Origins and bank transitions retain shared address checkpoints; after validation,
fragment fields retain final offsets, lengths and numeric operands. Callers can
still request the complete public fact array. The same
cache differential, work counters and allocation-failure tests exercise these
representation changes.

The linker supplies a private, link-lifetime heap to its values, layout and
object decoder. Fixed records share arena blocks, small allocations use reusable
size classes, and large temporary mappings are released individually. There is
no global allocator state. The caller destroys every object before destroying
its heap; the default APIs continue to use the C allocator. ASan poisoning keeps
freed small buffers inaccessible until reuse, and protects unused capacity.
Tests inject every mapping failure and check zero live allocations and mappings.
This changes storage ownership only, not evaluation or wire semantics.

Small blocks have no quarantine, so reuse can hide a later stale-pointer access
from ASan. LeakSanitizer cannot see individual pooled allocations; explicit
live-count/byte checks at heap teardown complement it. The default-libc paths
are tested too. Sanitizer silence is not a proof of pooled lifetime correctness.
Independent review measured the heap saving about 655 KB for dpadhero2 and
328 KB for DPadHero3Proto versus malloc, with similar speed; no simple saving
was found from mmap input. Peak RSS occurs during largest-object translation,
not at the post-layout allocation snapshot. See the validation record for the
trace and the larger wire-artifact sizes.

The driver protects the script, every object and every copied file from output
aliasing. It also detects duplicate destinations before they exist. It stages
and closes every output before publication. The linker streams the validated
fragment snapshot to these stages using the same encoder as buffered output,
avoiding an extra full-ROM allocation. Small fragments share a 4 KiB write
buffer; large runs write directly. Write failures, including a final partial
flush, leave publication to the caller's staging checks. It does no evaluation or input reads
during output writing. A rename failure can preserve only
the destinations not yet published; this is not a multi-file filesystem
transaction. No malformed object, unresolved value or range error publishes
an output.

On macOS the prototype assembler loads the Unicode comparison framework only
when comparing not-yet-existing names containing non-ASCII bytes. ASCII-only
names honor the volume's case-sensitivity setting without the framework.
Existing-file identity checks use filesystem metadata. Failure to load that framework fails the affected output
protection check; it never falls back to a weaker comparison.

Run `python3 tests/test_v2_objects.py` or `make check`. The object suite uses an
independent Python wire encoder, malformed objects, source/link controls,
large shared graphs, I/O injection and allocation-failure sweeps. The four v2
suites run under the Linux sanitizer CI job after publication. Only the small
wire example is distributed as test data; private project captures and tools
are not installed or shipped.

Source-checkout utilities `design/v2/check_object_projects.py` and
`design/v2/measure_object_projects.py` build in scratch copies from frozen assets,
select both tool paths explicitly, trap converter recipes and recheck input
hashes. The paired harness records separate object-assembly, linker and full
build samples and validates each ROM. These utilities do not replace the
independent review or the performance protocol.

Current status: all 15 required ROMs and diagnostics match after the review
follow-ups. The latest fixes restore W0201 for exported constants, discard leading
unnamed RAM storage even with a PC-dependent count, and skip RAM placement
when the script supplies no blocks while still checking sizes and required
addresses. Earlier fixes preserve export dependencies during label pruning,
keep deferred reassignment captures current, report missing layout errors and
name symbols in binding errors. The owner-approved strict script-number parsing is recorded in the
compatibility ledger and NEWS. Leaf sharing reduces object bytes by 12.8% for
dpadhero2 and 16.6% for DPadHero3Proto without a wire change. All project artifact
sizes and the remaining fixed-width tradeoff are in the validation record.
On 2026-10-25 the owner accepted
the current measured memory tradeoff for now, including DPadHero3Proto's
638,976-byte increase (30.7%) over v1. Keep per-tool accounting and review whether
the storage and existing complexity are justified; seek simple optimizations
without disproportionate machinery. Design section 6 and contract section 12
record this decision. Contract section 13 records bounded timing effort: the
small link deltas do not justify prolonged tuning. The reviewed candidate-14
15-project survey found
at most +0.562 ms link overhead and faster complete-build medians throughout.
Its formal plain Metroid/Zelda timing remains historical evidence. Current
check/distcheck pass 535/540 tests, Linux sanitizers pass, all required outputs
match. The reviewed short two-project cost survey remains historical evidence;
this warning/RAM follow-up makes no new timing claim.
Local independent review cleared I4 at `cf14315` with no blockers. The owner
approved I4 on 2026-10-25; see the record below. Leading unnamed RAM initializer `$` remains an
optional compatibility/diagnostic follow-up in TODO and the validation record;
it is not an approved contract exception.
See `design/v2/I4_VALIDATION.md` and prompt `design/v2/I4_REVIEW.md` in the source checkout.
The subsequent I4 approval below authorizes its PR and CI run.


## I4 approval record

Kent Hansen approved I4 on 2026-10-25 at
`08058227492c538341f080cd2274871893ab6d27`, after independent local review
reported no blockers on runtime revision
`cf14315345a932bd8948948393c77159130bcda0`. The intervening changes record
that review and the optional RAM-initializer follow-up; they change no runtime.
The owner authorized publication as the single I4 PR and its CI run.

The approval covers the isolated XOROBJ2 writer/reader, source object lowering,
linker integration, tests, and documented compatibility and resource decisions.
The optional leading unnamed RAM initializer `$` issue remains tracked for
follow-up; approval does not make it a permanent contract exception. PR CI,
I5 analysis/consumer migration and its performance gates, and the coordinated
I6 cutover remain required. Installed xasm/xlnk stay v1.

This approval record changes documentation only. The branch retains the
approved revision and adds this citable approval commit before publication.
