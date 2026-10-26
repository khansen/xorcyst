# XORcyst v2: approved D1 contracts

Status: **D1 approved by Kent Hansen on 2026-10-23 after independent local
review reported no blockers.** The approval record is in section 8; the
owner-approved binary-logical-operator erratum is in section 9.
These contracts implement the decisions in [the approved D0 scope](XORCYST_V2_DESIGN.md),
merged as `162dacc`. They define interfaces and acceptance cases, not C APIs or
runtime code. I1–I5 are approved and merged. The I6 candidate uses the normal tool names;
its review and release approval remain pending. The source-only baseline
record, `design/v2/BASELINES.md`, and
[performance protocol](design/v2/PERFORMANCE.md) are included in this approval.
The baseline evidence is not included in release archives or installed docs.

The fragment and fixup semantics were reviewed before the concrete wire layout.
The approved [wire layout](design/v2/OBJECT_WIRE_DRAFT.md) maps these entities
to records and a hand-written hex example. Its tags and byte offsets are part
of D1. Subsequent semantic or encoding changes must reopen the affected
contract for review. Implementation follows D0's staged sequence; this document
does not replace the installed tools or waive any implementation gate.

## 1. Boundary and ownership

Retain the existing parser and expansion behavior, opcode tables, integer
helpers and output staging. The frontend produces an owned unit containing
definitions, expressions, source locations, code fragments and RAM objects.
Direct binary and object linking consume the same semantic representation,
with the explicit placement/range policies in D0 §3.5. Analysis reads its
resolved facts; it does not reinterpret source expressions to calculate bytes.

The unit owns every definition, name, string and expression it references.
Dropping a parser statement, unused label, macro body or procedure does not
invalidate these references. A frontend-to-unit handoff must either transfer
ownership or copy into the unit; borrowed AST pointers are forbidden across
that boundary. Allocation failure is an assembly/link failure, not a partial
unit. A partially constructed unit can always be destroyed.

Unit-local indices suffice internally. A link references an entity by its
input-unit identity and local index; identical include filenames do not merge
definitions from separate units. No new public symbol-ID scheme is proposed.
Optional source trees and bindings retain the existing analysis identities
and macro use/definition distinction. Their storage is not built in plain mode.

## 2. Values, binding and evaluation

### 2.1 Results and dependencies

An evaluation request specifies the expression, its use/PC anchor, selected
definitions, output mode and layout epoch. It returns exactly one of:

| Result | Meaning |
| --- | --- |
| Known(integer) | A signed 32-bit expression value in this context. |
| Known(string) | A counted byte string, with no implicit terminator in its value. |
| Deferred(dependencies) | The selected definition or required placement fact is not yet available. No numeric placeholder is returned. |
| Invalid(diagnostic) | The expression is ill-typed, cyclic, out of bounds, or otherwise invalid. Its callers propagate failure. |

An address reference remains a dependency until its placement is known; it is
not silently treated as zero or as a constant's symbol-table index. `Deferred`
is legal during assembly of a relocatable unit. It is never legal in a requested
final byte, extent or resolved analysis field at publication. A legally unused
invalid/unresolved equate retains the existing null/omitted analysis behavior.

Memoize only within unchanged bindings and layout. A permanently known
constant needs no epoch check; an address-dependent result does. Preserve DAG
edges for shared aliases and scoped offsets. An active-evaluation stack detects
real cycles; revisiting a finished node is a cache hit. A field offset and its
containing type size are different dependency nodes. Union offsets are zero
without evaluating the union size. The cache must not suppress the first
located error or turn an invalid result into a successful cached value.

### 2.2 Existing binding is an input to the backend

This is not a new assignment-generation language. The existing frontend decides
which definition a use captures and which forward lookup remains pending.
Lowering stores that decision so subsequent layout does not look the name up
again in a different scope. It must retain:

- forward `.byte I / I = 1 / I = 2` -> `02`;
- capture before a later `=` for uses already reduced by v1;
- current `.undef`/redefinition lookup, including Nesrev's
  `pair_redefined_label` -> `B9 00 82`;
- definition-scope `@@` aliases, member tokens distinct from global constants,
  caller-scope indexes, and source-time `defined()`/`.ifdef` decisions;
- v1's different capture points for explicit `sizeof`, initialized instances
  and deferred uninitialized type allocation. An internal shared layout result
  is keyed by those bindings; it is not one globally cached size per type name.

Import annotations remain local type views, never provider-kind assertions.
Imports may resolve to integers, strings, code/procedure anchors or RAM anchors.
The provider's value kind governs evaluation. Keep the object-mode warning
`undeclared; assuming external label`; unresolved pure-binary names still fail.
Pin dpadhero2's `reset` use and a minimal consumer/provider case. Exported alias
dependencies use definition/expression references, never numeric local indices
that accidentally name another symbol.

### 2.3 Arithmetic and strings

All evaluators use [expr_int.h](expr_int.h): 32-bit wrapping add/subtract/multiply
and negation; signed comparisons; division truncated toward zero; remainder
with the dividend's sign; `INT32_MIN / -1 == INT32_MIN` and remainder zero.
Right shift is logical over 32 bits, including count zero; both shifts require
0..31. Zero divisors and invalid counts are Invalid. Low/high-byte extraction,
bitwise operations, unary logical NOT and comparisons keep v1 meanings. Address/file
extent bookkeeping uses checked host-independent counters rather than wrapped
expression arithmetic.

Resolve value kinds before simplifying. Integer `x+0` may fold only after x
is known to be integer. Strings concatenate with `+`; mixed operands convert
the integer to signed ASCII decimal with no leading plus, padding or locale.
Comparison is bytewise lexicographic, preserving v1. A string needed for a data
extent must be known before placement; one requiring a later address or a
relaxation result is rejected. This includes decimal conversion of such an
address. There is no iterative string-length solver.

The existing scanner recognizes double-quoted, single-line strings and passes
the bytes between the quotes through; it does not interpret C escapes in string
literals. Character-map files have their own escape handling. Keep that
distinction. Raw NUL input truncation is a separate known input defect, not a
new binary-literal syntax. Counted values must nevertheless preserve zeros
introduced by a character map.

In data, each string byte becomes one element of the directive's width:
`.byte`/`.char` use one byte, `.word` two and `.dword` four, with zero high
bytes. The extent is string length times element width, checked before
allocation; it is not always the string length alone. `.char` maps each input
byte exactly once using the captured map. Other data types do not apply that map.

Resource policy: a materialized string is at most 16 MiB;
the total live unique string payload per evaluation unit is at most 256 MiB.
Shared values count once, concatenation checks length before allocation, and
object loading enforces the same limits before allocation. These limits protect
the demonstrated doubling-chain case; they are not a truncation rule. Exceeding
one reports a located resource error and keeps old outputs. No 16,384-expression
ceiling carries forward: table counts are limited by checked representation and
available memory. Retain current frontend macro/loop/include limits; use an
explicit evaluation stack so graph depth is not a C stack limit. These string
budgets are included in D1 approval; implementation validation uses the frozen inputs.

## 3. Fragments and operand fixups

This is the minimum logical unit model, not a prescribed C class hierarchy.

| Entity | Required information and semantics |
| --- | --- |
| Definition | Name/binding identity, kind, expression or anchor, export visibility, diagnostic location; importer-local type information stays local. |
| Expression | Typed literal or operator, operand references, definition/anchor references, location. Shared nodes remain shared. |
| Anchor | Boundary or operand-start within an ordered fragment sequence, plus segment/unit identity. Procedure and variable addresses use the same resolved-address facts as labels. |
| Raw data | Counted bytes and source location. May coalesce physically only while preserving source spans for diagnostics/analysis. |
| Data operand | Expression, element width (1/2/4), string permission, use anchor, and optional captured `.char` map. String expansion uses length times element width; numeric operands retain their width and bypass the map. |
| Instruction | Final/selectable opcode family, explicit `.w` constraint, operand expression/fixup, PC anchor, range-policy origin, and optional analysis provenance. |
| Storage | Extent expression, element scale, permitted minimum, source location and use anchor. Distinguish standalone allocation from an initialized field remainder. |
| Field remainder | Declared count and initialized extent retained separately for validation, scale/type binding, minimum zero and field location/name. A negative declaration is invalid even if subtraction/coalescing would hide it. |
| RAM object | Per-data-label source span (including unlabeled storage), alignment, zero-page requirement, v1 label order and location. Placement does not treat all data in a unit as one section. |
| Placement directive | Pure-binary origin/segment changes, or existing linker script copy/bank/link/pad/RAM operations. Raw-copy source identity and length are captured before layout. `.org` is never serialized as an object feature. |

The frontend must keep label/data boundaries until each source-time
`sizeof(label)` has either produced an integer or an error. Its scan can cover
several statements and includes the fixed width of its own numeric value operand
when inside that scan. A string-valued operand built from that size rejects,
as specified in section 4.1. RAM allocation separately keeps emitted data-label boundaries
to determine allocation extents. These two uses of boundaries are not the same
size calculation; section 4.1 defines each. Neither depends on raw-byte coalescing.

A fixup belongs to the specific data or instruction operand. It carries its
expression, PC rule, width/range rule and location. The writer must not infer
the rule from whatever instruction node happened to survive lowering. Each
final instruction fact records the emitted opcode, length, operand value and
bytes; the v2 listing and records consume that same fact.

`$` in an instruction operand is the instruction's start address. Branch
encoding subtracts start+2 from the resolved target. In a multi-operand data
directive `$` is the start of that operand, after preceding strings or numbers.
In ordinary storage it is the storage statement's start. A field dimension
containing `$` in the type body is rejected when the type declaration is
completed, including an unused type. This deliberately rejects unused declarations
that v1 accepted. Equates and `.define` capture `$` at their own definition;
using such a fixed value in a dimension is allowed (`nop / P .equ $+1 /
.struc S / f .byte[P] / .ends` has a two-byte field). There is no deferred
instance-PC check through those aliases. Existing errors for an equate capturing
an unavailable PC still apply.

Two v1 tests must migrate from accepted-byte assertions to located errors:
`test_nested_pc_counts_document_object_merging_difference` and
`test_omitted_arrays_keep_pc_dependent_field_evaluation_positions` in
`tests/test_field_initializers.py`. Add an unused-type rejection and the fixed
PC-equate positive control; do not silently delete these cases.

At expansion directives, `$` requires a final absolute PC at that source point.
A preceding unknown width/extent, or an unplaced object origin, makes it
unavailable. Report the located error even for syntactically cancelling uses
such as `$-$`; no provisional PC is reconstructed to retain v1's accidental
branch. `defined()`/`.ifdef` retain their source-time namespace semantics.

## 4. Layout and final validation

### 4.1 Extents and placement

For layout of an already-lowered unit, bind exports/imports and resolve
non-layout values, including strings, first. This does not retry frontend
source-time queries such as `sizeof(label)`.
Resolve RAM extents that do not require unplaced RAM addresses before allocation.
For each unit, use v1's emitted data-label order (including retained local and
anonymous labels), then concatenate those arrays in unit link order. Compute
v1's unsigned `label_sort_key` and apply its exact last-pivot Lomuto quicksort
permutation, with the `>=` partition comparison, before first-fit allocation.
This is **not a stable sort** and source order is not a tie-break. The exact
pseudocode and key are in the wire draft's RAM section. An alternative sorting
implementation is allowed only if it produces the same entire permutation.
Preserve source-ordered RAM addresses in pure binary. These remain separate
placement inputs, as D0 specifies.

A RAM label owns the virtual source span from its boundary to the next emitted
data-label boundary, or the end of that unit's data segment. Include every
unlabeled storage statement in that interval. Compute its byte extent before
physical placement: subtracting two allocated RAM addresses is incorrect because
allocation can reorder the labels. A bare label followed immediately by another
label or segment end has extent zero. Preserve v1's zero-size allocation: select
the first suitable RAM address under its constraints without consuming bytes;
this is not permission for standalone `.dsb 0`. With an unconstrained block at
$10, `aa .byte / .dsb 3 / bb .byte` places bb=$10 and aa=$11; `buf: / aa .byte /
bb .byte` places buf=$10, aa=$10, bb=$11.

For **every** `sizeof(label)`, whether the label is in code or data, retain
v1's assembler-time scan (`sizeof_label_data`) at the point where the `sizeof`
use is processed. The label must already be defined and every storage count in
the scan must be known there. Otherwise report the existing located
“size of `X' is unknown” error during assembly in both output modes. Do not retry
with later definitions or import bindings, wait for layout/linking, or substitute
the RAM allocation extent. The limited unit-end operand-kind check below does
not retry the scan or its storage counts. A successful label-size use becomes a
plain integer literal in the unit/object; it has no deferred span expression.

Scan from the existing label's declaration in the current frontend statement
sequence, including contiguous data, storage and padding, and skipping internal
scope markers. After data begins, another label ends the scan. Before any data,
adjacent labels share the following data scan. Preserve the distinction between
statements already consumed/expanded and boundaries still present at this use:

- `.equ`, `=`, `.define`, `.undef`, `.extrn`, `.message`, `.warning` and macro
  definitions have been consumed and do not end the scan. `.if`, `.ifdef`,
  `.ifndef`, `.else` and `.elif` contribute only the selected branch; `.rept`,
  `.while`, `.do`/`.until` and macro calls contribute their expanded statements.
  Their wrappers do not end the scan. Count their data, and respect any actual
  boundary produced by the expansion. `.exitm` contributes only the macro data
  expanded before the exit; a label produced by a macro still ends a scan that
  has already found data.
- Instructions, procedures, segment switches (even a redundant `.codeseg`),
  `.public`, `.struc`, `.union`, `.record`, `.enum`, `.label`, `.charmap`, `.align`,
  `.include`/`.incsrc`, `.incbin` and `.org` end the scan. Keep these
  scan boundaries through lowering. In particular, an include ends it even if
  the included file contains only data; a macro call containing the same data
  does not. `.org` remains pure-binary-only.

Default: after the data/storage, label and internal-marker cases above, any
other statement still present in the frontend sequence ends the scan. An
unlisted directive is not implicitly skipped; an already consumed statement
is absent, and an expanded construct is judged by the statements it leaves.

Do not append storage from a later return to `.dataseg`. Preserve v1's
error/fallback when no data is found and its separate type/variable `sizeof`
capture rules. This is not a general code-size operator and does not measure
placed addresses; whether a directive itself emits bytes is not the stop rule.

String byte lengths in this scan must also be known at the use. On 2026-10-23,
the owner chose a located `size unknown` rejection for a string still unresolved
at the `sizeof` use, or supplied by an import. Do not count such a string as one
element, and do not defer its size to the linker. A string unresolved at the data
statement but defined before the `sizeof` use has a known length at that use:
`Msg: .char S / S .equ "HELLO" / .byte sizeof(Msg)` yields
`48 45 4C 4C 4F 06`. This deliberately corrects v1's linked `... 02` and binary
rejection. Previously resolved operands retain their existing captured bindings;
this is not permission to recapture a value after reassignment.

On 2026-10-24 the owner clarified the unit-end guard for operands unresolved
at their data statement: record the kind and, for strings, the byte length
counted by each `sizeof` scan. Validate against the operand's final binding,
including `=` reassignment and `.undef` replacement. If that kind or length
changes, report size unknown at the original `sizeof` use; do not recompute its
captured integer. Numeric value changes and same-length string replacements
remain valid. Literal values already captured at the data statement retain
their captured width. This check also applies when the operand has become a
known string or integer by the time of the scan.

On 2026-10-24 the owner separately chose rejection of string-valued data
operands built from the containing label's own `sizeof`, even without
reassignment or `.undef`. `Tbl: .db sizeof(Tbl)+"A"` reports size unknown at
the sizeof use and preserves the previous output; v1 emits `31 41` using the
stale size 1 for two bytes. Count an own-size operand as one element only if
its complete result is numeric. Numeric arithmetic and captured numeric
constants remain valid. Labels sharing the same leading data run share this
rule. Sizes of unrelated labels still reduce normally, so
`Other: .db 1,2 / Tbl: .db sizeof(Tbl) / .db sizeof(Other)+"A"` emits
`01 02 03 32 41`. Do not solve a self-sized string by iterating layout or
retain v1's stale size. This is a distinct migration decision from the
reassignment guard above and applies before binary or object publication.

The owner also selected preservation of forward **local** operands on
2026-10-23. Count each such unresolved data operand as one element of its
directive's width, as v1 does, and retain an assembler-only check associated
with the `sizeof` use. Before the unit can publish binary or object output,
validate its kind under the existing binding rules. A local integer/address
keeps that fixed-width contribution, even if its final numeric address is not
known. If it resolves to a string, cannot establish a local non-string kind,
or remains undefined and becomes an implicit import, report size unknown at
the original `sizeof` use. An explicit import is rejected too, including
annotated `: byte` or `: proc` imports: annotations do not guarantee the provider's
kind. Follow local alias chains too: an operand that still depends on an import
is rejected whether its local alias is defined before or after the table or the
`sizeof` use. A locally declared alias does not turn an imported value into a
local one. This check never runs in the linker, changes the captured size, revisits
the scan, or makes a forward storage count valid. There is no serialized type
guard. Local numeric forward references and jump tables thus retain v1's bytes;
imported pointer tables deliberately become errors. The D1 ledger records both.
Ordinary forward/imported string data without `sizeof(label)` still gets its
resolved length under D0, including correct later label addresses and character
maps. Known strings keep their normal widths: `Msg: .char "HELLO" / .byte
sizeof(Msg)` emits `48 45 4C 4C 4F 06`, with integer 6 captured by the frontend.

Required source-time controls in both modes:

| Source | Result |
| --- | --- |
| `.dataseg / p1: / p2: / .dsb 2 / p3 .byte / .codeseg / lda #sizeof(p1)` | `A9 02`, although p1's allocation extent is zero. |
| `.dataseg / buf: / .dsb 2 / .codeseg / lda #sizeof(buf) / .dataseg / .dsb 3 / nxt .byte` | `A9 02`, although buf's eventual allocation extent is five. |
| `.byte sizeof(Tbl) / Tbl: .db 1,2,3` | Located size-unknown error; forward label sizeof is not added. |
| `Buf: .dsb N / .byte sizeof(Buf) / N .equ 3` | Located size-unknown error, likewise with `.extrn N : byte`. |
| `Tbl: .db 1,2 / K .equ 5 / .db 3 / .byte sizeof(Tbl)` | `01 02 03 04`; the consumed equate does not end the scan. |
| `Tbl: .db 1,2 / .public Tbl / .db 3 / .byte sizeof(Tbl)` | `01 02 03 02`; the public directive remains a scan boundary. |
| `Msg: .char S / S .equ "HELLO" / .byte sizeof(Msg)` | `48 45 4C 4C 4F 06`; length is known at the sizeof use. |
| `Msg: .byte Value / .byte sizeof(Msg) / Value .equ 7` | `07 02`; the unit-end check confirms the local integer kind. |
| `Tbl: .dw H1,H2 / .byte sizeof(Tbl) / H1: rts / H2: rts` at origin 0 | `05 00 06 00 05 60 60`; forward local addresses keep word width. |
| `.extrn H : proc / Tbl: .dw H / .byte sizeof(Tbl)` | Located size-unknown error even with a valid provider; likewise if H becomes an implicit import. |
| `.extrn Ext : byte / K .equ Ext / Tbl: .db K / .byte sizeof(Tbl)` with provider Ext=7 | Located size-unknown error, also with `K .equ Ext` after the sizeof use. Both change v1's linked `07 02`. |

Keep the earlier controls: unlabeled data belongs to a scan; global/local/
anonymous labels and procedures end it after data; adjacent labels before data
share it; instructions end it; a known storage count and known strings succeed.
The RAM extent/placement rules above remain independent of these captured values.

Represent code-anchor differences within one contiguous placement run as the
sum of the intervening fragment extents. Common unknown prefixes cancel; RAM
objects and unrelated origin/bank runs do not have that common prefix. Thus
`Tail-Head` across two NOPs is known as 2 even if a storage fragment before Head
uses it. `Tail-Start` across storage whose size depends on that same difference
has a real dependency cycle and is rejected, rather than iterated until some
arbitrary fixed point. This is span evaluation, not a general equation solver.
Counts depending on ordinary already resolved addresses retain existing rules.

Evaluate all extent constraints before summing neighboring storage. Standalone
counts keep their existing positive rule and 1..65535-byte bound; field
remainders allow 0..65535 bytes. Composite extents preserve the original
per-count checks instead of applying the standalone maximum to a coalesced run.
Large literal runs may still split at encoding boundaries. No negative inner
count can be hidden by positive neighbors; an empty allowed outer field does
not instantiate inner elements and therefore does not validate nonexistent
inner storage. Literal zero/empty-type behavior outside the selected remainder
fix remains pinned to the existing initializer tests.

The final writer uses exactly the extents that assigned addresses. It does not
re-evaluate a count at a different PC or silently change a size. Copy inputs
are checked for the captured size/identity before publication. Object mode
continues to reject RAM extents requiring unplaced RAM labels; direct binary
retains its existing source-order support (the second D0 parity exception).

### 4.2 Narrowing loop

Preserve v1 xlnk's **in-place, source/script-order** narrowing and address walks.
Do not use trial layouts that can decline a constant zero-page instruction to
rescue another operand. Resolved constants and placed RAM/imports get their
widths before code placement. Explicit `.w` and instructions without a legal
zero-page counterpart stay wide. For a layout-dependent operand, shorten only
when its currently available value is in 0..255 and the opcode has a narrow
equivalent. Once shortened it is never widened.

Every walk, including the first, narrows each candidate whose operand is
available at that point, in walk order. The first walk also establishes forward
anchors; suppressing premature unresolved-operand errors does not suppress
narrowing. Subsequent walks use the
last available forward addresses and immediately updated backward addresses,
as xlnk does; this is not a simultaneous-equation update. Repeat while a walk
shortens any instruction, then perform a stable address/extent validation walk.
With C remaining candidates, at most C walks can report a new shortening;
allow one initial and one final unchanged walk. A per-walk expression cache
must not reuse a value after an address it depends on changes within that walk.
Implementations may use dependency versions or invalidate conservatively;
the minimum contract is correctness, not an obligatory cache framework.

With fixed resolved extents, the fragment walks are bounded by O((C+2) ×
fragments). Expression work additionally includes edges revisited after a
binding or depended-on anchor changes, plus walks collecting unresolved
leaves. Shared expressions must remain shared and unrelated placement updates
must not discard their memoized values. There is no universal linear bound
per walk for arbitrary shared operands under this in-place update order:
measure evaluated nodes, invalidation edges and dependency visits as well as
walks/fragments. Pin both an ordinary shared-operand case and an adversarial
many-anchor case with explicit work budgets. Independently known widths incur
no candidate trials. This corrects the earlier O((C+2) × (fragments + reachable
expression edges)) claim; section 10 records the owner's approval. It does not
relax the required project no-slowdown gate.

Dependent extents must resolve through the acyclic extent graph for each walk,
not create a second unbounded retry loop. If no width changes but required facts
remain Deferred or final extents disagree, fail at the dependency's location.
The performance protocol includes a long cascade to expose the worst case.

Check every final operand, branch, data range, count, alignment, bank, origin,
pad and output extent; analysis sees only the validated final layout. Keep the
existing literal/early truncation behavior and warning policy. Late zero-page
operands must fit 0..255; byte immediate/indirect operands fit -128..255, and
relative displacements -128..127. Late word-instruction operands retain binary
warn/truncate versus link rejection outside -32768..65535. `.byte`/`.word` data
warn and truncate on both paths. Keep v1's binary address-space policy and the
linker's 64K check: binary PC/label values themselves remain unwrapped past
$FFFF, while 16-bit emission/display truncates under the existing range policy.
For example `.org $FFFF / .byte $EA / Here: .dword Here, $` gives
`EA 00 00 01 00 04 00 01 00`. The earlier phrase “PC wrapping” did not mean
wrapping anchors at 16 bits; D0 did not authorize that byte change.

The exact five D0 §3.4 controls and both first-walk controls below are mandatory. In particular the $FA example
stays wide for L, the $F6 cascade gains its second shortening, and `lda T-3`
fails after narrowing. A different globally smaller or error-avoiding result
is not an acceptable optimization of this contract.

At origin 0, `Back: nop / lda Back / lda K-L / nop / L: rts`, with
`K .equ 263` defined first, must emit `EA A5 00 AD 00 01 EA 60` on both
paths. With K=262 it must reject on both paths. Leaving `lda Back` wide on
the first walk changes later irreversible decisions and violates this contract.

## 5. Object contract, before wire numbers

An object contains one owned, expanded unit: code fragments, RAM objects,
definitions/exports, imports, a shared expression graph, counted blobs/maps,
operand fixups and diagnostic file/line/column locations. It needs no parser
AST, source re-expansion, cross-unit type definitions or diagnostic-event table.
Unresolved field/scoped operations must already have been lowered to their
actual dependencies, not escaped as unsupported AST tags.

Use one portable fixed-width little-endian encoding with an unambiguous new
magic/format discriminator; reject v1 with a rebuild message. Counts/indices
are unsigned 32-bit fields, integer literals are 32 raw bits, and aggregate
file offsets/length arithmetic is checked in a wide counter before narrowing.
Counted string/byte payloads permit zero. Index zero is valid; optional references
must have an explicit absence flag rather than stealing a valid entity index.
No pointer dumps, host `long`, count-minus-one field storage, 16,384-expression
ceiling, or arbitrary optional-extension framework.

Before serialization is frozen, the D1 wire checklist must show byte layouts
for each logical entity in §3, file header lengths/counts, reference validation,
and at least one independently decoded hex fixture. Review it against the
approved fragment semantics first. Unknown tags, truncated tables, duplicate
exports, invalid references, inconsistent extents, integer-overflow lengths and
cyclic value dependencies fail before output publication. Shared DAG nodes
round-trip without tree expansion. Test byte-swapped inputs and 32-/64-bit-host
decoding without making a new emulated platform a release prerequisite.

Normal objects retain locations for definitions, operands and storage, even
without rich debug output. Optional source text remains optional. The linker
may use the owner location for a reference/address leaf instead of a redundant
expression span; operand fixups still retain their use-site locations. Literal,
operator and bank-expression spans remain present in the current writer.
These location-preserving choices do not change the wire record layout or add
an object-mode analysis consumer. The linker
reports location unavailable honestly if an imported file lacks usable metadata;
there is no new stripping mode. `^Symbol` uses its owning/provider unit's bank
index, or integer -1 when absent. `copy{file}` embeds raw file bytes. Neither
gets a new pure-binary or analysis feature.

## 6. Analysis, schemas and publication

Start the single structured listing from current listing **version 2** in
JSON and NDJSON, preserving its identifier and field meanings. Remove the
selector at coordinated cutover; do not renumber the product merely because
the executable becomes v2. Object-mode structured listing/xref requests fail
clearly. Object text listing is optional only through the shared renderer.

Keep inclusive listing `*_end` fields, null offsets for non-emission, explicit
fill metadata and the opcode-derived addressing mode (including operandless
accumulator shifts). Do not globally change coverage fields that already use
`*_end_exclusive`. Keep existing JSON number/string choices and resource limits;
there is no new 64-bit JSON format. Physical PRG page = output_offset / 16384
is distinct from the linker's bank identity. Copied source files retain their
own file identity, not the including statement's identity.

Retain xref version 2 and instruction records version 3 with its shared file
table. Preserve reference filters, exclusion of member tokens, scoped enum
visibility, deterministic ties, source/binding identities and additive terms.
Terms, operand values, emitted values and listing bytes come from the same
resolved facts. A CPU address and a raw output offset are separate coordinates;
macros and repeated bank CPU windows do not make either a unique source key.

The consumer checklist in `design/v2/BASELINES.md` in the source checkout (not shipped) covers every
D0 output product. Port the collectors, not their schemas by default. Nesrev's
bundle reader must accept listing version 2 and its corrected non-emitting/fill
facts, and its fixture expectations must migrate intentionally. A producer
identity change invalidates cached bundles. Keep records-v4 deferred. No xlnk
analysis/debugger outputs are introduced.

Plan all requested destinations before publishing, including NL bank files.
Evaluate/validate bytes and required facts, write temporary files, run the
pre-rename completeness check, then publish with the dependency manifest last.
Any Invalid/unresolved required value preserves previous binary, listing,
xref, records, manifest and NL files. Diagnostic listings retain the existing
exception only when their destination plan is safe, with no emitted-byte claim.
Keep the documented unresolved-ROM-layout refusal; do not weaken destination
protection to obtain a diagnostic listing. Multi-file rename is not globally
atomic; Nesrev still verifies hashes and completeness. Fault injection must
test a successful writer with deliberately incomplete analysis independently
of natural source errors.

## 7. Review and acceptance cases

Use exact inputs and expected results from D0 §3.5 and the following additions.
Run both modes only where supported and placement-equivalent; no v1 objects
are used as v2 fixture inputs.

| Group | Required assertions |
| --- | --- |
| Binding | Forward/reassigned I=02, captured earlier uses, `.undef` Nesrev table `B9 00 82`, definition-scope local aliases, member/constant collisions, `.ifdef` false before definition. |
| Types | Acyclic self-offset, real direct/mutual size cycles, zero union offsets, mutable counts at existing capture points; direct instance-PC fields rejected at declaration even when unused; fixed PC equates remain valid. |
| Integers | Existing wrapping, exported negatives, mixed label expressions, root multiply, invalid shifts and zero divisors; all analysis values agree. |
| Strings | Forward ABC moves After to 3; S+0 -> `41 30`; imported/forward HI with map -> `01 00 01 00`; use-site map reassignment, numeric `.char` bypass, exponential value limit and mapped NUL. |
| Exports | Provider `Alias .equ Value / Value .equ 7 / .public Alias`, consumer `.extrn Alias : byte / .byte Alias` -> `07` in either link order. A provider alias to an external constant retains that dependency through a third unit. |
| Implicit externals | Consumer `jsr reset`, provider public `reset: rts`: object warning retained and linked operand points at provider; binary without definition fails. Required game warning remains. |
| Extents | Independent Tail-Head -> `00 00 EA EA 60`; negative count adjacent to positive storage rejects; imported N=2 zero char-field remainder -> `41 42 07 FF`; genuine self-dependent extent rejects. |
| Layout | Five D0 narrowing controls plus first-walk K=263/K=262 controls, exact RAM quicksort permutation, RAM allocation spans and source-time label sizes, RAM small/tiny/big placement, banks/pad/copy/alignment, branch edge ranges, `.w`, same bytes/acceptance in equivalent contexts except the two named policies. |
| Analysis | All byte/operand/term/value agreements; late fill spans; missing/unresolved diagnostic values; complete profile output sets; FCEUX and path collision protections. |
| Objects | Logical round trips, sharing rather than exponential expansion, malformed/truncated references and payloads, allocation failures, old-format rebuild error, location-bearing link failures. |
| Label sizeof | Source-time RAM/forward-count cases; consumed/expanded statements versus surviving scan boundaries; a string defined before the query uses its real length; a string still deferred at the query rejects. Forward local numbers/addresses pass the unit-end kind check; explicit/implicit imports reject. Known strings/counts capture literal sizes. Preserve previous binary/object outputs on every rejection. |

Also require these three implementation tests for the scan behavior already
specified in §4.1. The local reviewer confirmed them on v1; they are not part
of the recorded 50-case/200-run capture:

- With `Tbl: .db 1,2`, a macro that emits `.db 9`, then `.exitm` before `.db 8`,
  followed by `.db 3 / .byte sizeof(Tbl)`, emits `01 02 09 03 05`.
- With the same table, a macro expanding to `Inner: .db 9`, followed by
  `.db 3 / .byte sizeof(Tbl)`, emits `01 02 09 03 02`: the expanded label ends
  the scan after the original data.
- `Tbl: .db 1,2 / .codeseg / .db 3 / .byte sizeof(Tbl)` emits `01 02 03 02`:
  even the redundant segment directive ends the scan.

Run these in both output modes with equivalent placement when implementing the
frontend. These are test obligations, not claims that v2 tests have run.


### D1 migration clarification ledger

These entries supplement D0 §3.5 and are included in the section 8 D1 approval.

| Case | v1 behavior | D1 disposition |
| --- | --- | --- |
| Linker-script numeric arguments with trailing text or no digits, e.g. `origin=8000h`, `$C000x`, `1e3`, bare `$` | `strtol` accepts a prefix: respectively decimal 8000, hexadecimal C000, decimal 1 and zero. | **Owner-approved correction, 2026-10-25.** Require the complete numeric token and reject malformed arguments with a located script error, preserving existing outputs. Keep valid decimal, hexadecimal and binary spellings; script commands and placement semantics otherwise stay unchanged. The prototype README/NEWS and source/link tests record this correction. |
| Forward data reassigned after its `sizeof` use: `Tbl: .dw S / S = "ABCD" / .byte sizeof(Tbl) / S = 7`; string-length changes and `.undef` replacements too | Both modes emit `07 00 03` for this example. Other changes can reject in binary mode or link with a stale size: `Msg: .char S / S = "HELLO" / .byte sizeof(Msg) / S = "HI"` links as `48 49 02`. | **Owner-selected guard, 2026-10-24.** Reject at the original sizeof use if the final operand kind or string length differs from the scan's contribution. This deliberately rejects some v1-accepted programs rather than publishing a stale captured size. Stable numeric kinds, same-length strings, and values already captured at the data line remain accepted. Source tests preserve the old output on rejection; I4 must apply the same guard before object publication. |
| A string built from its containing label's own size: `Tbl: .db sizeof(Tbl)+"A"`, including nested arithmetic and captured string prefixes | Both modes emit `31 41`, with stale size 1 for a two-byte table. No reassignment or `.undef` is needed. | **Owner-selected rejection, 2026-10-24.** Report size unknown at the sizeof use and preserve previous output. The whole operand must keep its fixed numeric width; a string result rejects even when sizeof itself returns an integer. Numeric forms and strings built from unrelated label sizes remain supported. I3 source tests, NEWS and section 4.1 record this deliberate compatibility change; I4 must enforce it before object publication too. |
| Source-time `$` after a narrowable instruction, in `.if`, `.equ`, `.define` or `=` | After `lda $10`, v1 uses provisional PC 3 even though the instruction emits two bytes. `lda Fwd / Here .equ $ / .dw Here / Fwd .equ $10` captures 3 too. | **Owner-confirmed correction, 2026-10-24.** D0 §3.4's known-final-PC rule means the first case uses 2; `.if $==2` can now select its body, and a captured value emits `02 00` rather than `03 00`. Unresolved instruction widths or forward data/string extents reject source-time PC uses with a located error. Operand-time `$` still uses its final anchor. No frozen-project occurrence was found in the review's scan; project builds remain the acceptance gate. |
| Multiple string operands in one `.char`, with H mapped to 1 and I to 0 | `.char "HI","HI"` emits `01 00 48 49`: v1 maps only the first string. | **Owner-confirmed correction, 2026-10-24.** Section 2.3 applies the captured map to every string operand exactly once, giving `01 00 01 00`. Numeric operands bypass the map. Tests cover multiple strings, forward strings and map reassignment. |
| String operands in integer identities: `"abc" * 1`, `"abc" / 1`, `1 * "abc"`, including deferred S in `S * 1` | The three known-string cases emit `61 62 63` in both modes through identity folds. Forward S rejects in pure binary but links to those bytes when S resolves to "abc". | The existing D0 §3.2 kind-before-identity rule rejects these as type errors. This is a deliberate correction in addition to the already recorded `S+0` change, not string multiplication/division support. I1 pins the value-level errors; I3 must test located rejection and output preservation in both source paths, with known and forward strings. No literal-string uses were found by the source scan; identifier forms must be checked by the I3/I4 project builds. |
| `Msg: .char S / .byte sizeof(Msg)` with S defined later as "HELLO", or imported; also `Msg: .char S / S .equ "HELLO" / .byte sizeof(Msg)` | Binary rejects both orders; object linking can emit `48 45 4C 4C 4F 02`. A following label can also have a stale address. | **Owner-selected rejection when still unresolved at sizeof (2026-10-23).** Reject both modes with size unknown for the first order/import. In the second order the length is already known at sizeof: emit `48 45 4C 4C 4F 06` on both paths. This latter correction is explicitly part of D1 review. No link-time label sizeof; ordinary data retains D0's forward-string support. |
| Data operands in a label-size scan whose kind is unknown at the use, including local aliases of imports | A later local `Value .equ 7` gives `07 02`; a forward local jump table gives `05 00 06 00 05 60 60`. Imported numeric/address operands also link successfully. `K .equ Ext` used in the table gives linked `07 02` with provider Ext=7 whether K is defined before or after the table/sizeof. | **Owner-selected 2026-10-23: preserve local forward operands, reject imports and deferred strings.** Count one element provisionally, then validate local non-string kinds before unit publication; diagnose failures at sizeof. Explicit imports, names that become implicit imports, and local alias chains ending in either reject in both definition orders, even when a provider would supply a number/address. No serialized guard. Data without sizeof is unaffected. |
| `.label name = expr` (explicit address) | Code honors the address only when linked; data ignores it and can produce a negative allocation extent. The manual documents the form. | **Owner-selected 2026-10-23: reject with a located unsupported-form error in 2.0.** No required frozen project uses it. At implementation, update the Global labels manual syntax and NEWS/release notes, and add a located-rejection/output-preservation test. Ordinary `.label name` and its local type annotation remain supported; no fixed-address wire flag. |

The independent local reviewer cleared the scan rules, ledger and wire changes
with no blockers. No v1 behavior is changed by this document. The source
inventory and v1 observations are in the local evidence.

I2 also pins the already specified signed branch range: v1 pure binary wrongly
accepts a late +128 displacement (`bne Target / .dsb 128 / Target: nop`) and
emits `D0 80`, whereas xlnk rejects it. The shared -128..127 rule above rejects
this incorrect binary too; I3 must preserve the located error/output gate in
its source tests. This records a consequence of the approved branch policy,
not a wider relaxation of operand ranges.

## 8. D1 approval record

**D1 approved by Kent Hansen on 2026-10-23.** After independent local review
reported no blockers, the owner answered "yes" to approving D1, committing it,
opening one PR and merging after green CI.

Approval covers this semantic/analysis contract and its migration ledger, the
performance protocol, the 15-ROM frozen gate and indexed baseline evidence, and
the concrete wire layout with its single 244-byte example. It includes the
three implementation-test obligations added in section 7 following review.
The approval applies to the complete D1 revision in the commit introducing
this record; its PR records the branch commit and resulting merged commit.

D1's review and owner-approval gate is complete. After this revision is merged,
I1 can begin under D0's implementation sequence, one implementation PR at a time.
Installed tools remain v1 through I5. Approval does not waive dpadhero2, the
other required ROMs, complete Nesrev consumer validation, the no-slowdown gate,
or the final cutover/release gates. No v2 runtime implementation or release is
part of this revision.

## 9. D1 erratum: exclude binary logical AND/OR

**Owner-approved 2026-10-23.** Local I1 review established that binary `&&` and
`||` have no v1 source syntax or object codes and no uses in the 2,887 frozen
source files. Their inclusion in wire tags 42–43 and the first I1 value API was
unsupported scope, not an existing expression feature. Kent Hansen approved
removing them and reopening this part of the D1 contract to reserve those tags.

Remove the two operators from I1. Wire tags 42–43 are reserved and invalid on
input; preserve all other wire tag numbers. This correction precedes any v2
runtime object writer or reader and leaves the single 244-byte example
unchanged. I4 must reject objects carrying either reserved tag. Unary logical
NOT and the existing bitwise operators remain supported. No new syntax or
semantics are approved by this erratum; the other D1 decisions and gates stand.

## 10. D1 erratum: account for repeated expression work

**Owner-approved 2026-10-23.** I2's direct-IR probe constructs a shared operand
from 100 or 200 code anchors, using it at each instruction while those anchors
change in the mandated in-place walk. The initial implementation measured
10,800 / 41,600 evaluated nodes respectively. The earlier universal linear
per-walk expression bound was too strong. Kent Hansen approved correcting it
to account for actually revisited expression edges, retaining the strict
project performance gates and adding an adversarial regression fixture.

This changes neither evaluation order nor source semantics. I2 reports work
counters and tests bounded examples; I3–I5 must still demonstrate no slowdown
on every required project/configuration under `design/v2/PERFORMANCE.md`.
A synthetic counter test is not a substitute for Metroid, Nesrev analysis,
dpadhero2 or the other frozen project gates.

## 11. D1 clarification: deferred initializer operand lengths

**Owner-approved 2026-10-25.** Wire extent tag 8 counts one element for a
numeric operand and the counted byte length for a string operand. An imported
initializer's provider supplies its kind. Both kinds use their actual extent
when checking field remainders; neither leaves the provisional one-element
string padding that v1 emits. This clarifies the existing forward-string and
provider-kind rules without adding a tag or source syntax. Explicit
`sizeof(label)` keeps its assembler-time rules and has no wire operation.

## 12. I4 memory tradeoff

**Owner-approved 2026-10-25.** After reviewing where the additional storage goes,
the owner accepted the current measured I4 memory overhead for now and requested
that the design and reviewer prompt state the tradeoff explicitly. The focused
standalone-link measurements include dpadhero2 at +753,664 bytes (+28.6%) and
DPadHero3Proto at +638,976 bytes (+30.7%). The latter exceeds the earlier
below-30% per-tool limit and is explicitly accepted; it is not a measured pass
under that limit. This does not create a general absolute or percentage allowance
for further growth.

Continue per-tool accounting and review of purpose, capacity and lifetime.
Reviewers should seek simple optimizations with demonstrated benefit and assess
whether existing complexity is justified. Extra allocator/encoding complexity
solely to shave small amounts is not a goal. Design section 6 and
`design/v2/PERFORMANCE.md` state the policy; the source-checkout validation record
and `design/v2/I4_REVIEW.md` carry the measurements and reviewer instructions.
This decision does not approve I4, waive timing or byte-exact project gates,
or authorize publication before local independent review.

## 13. I4 timing effort and absolute costs

**Owner guidance, 2026-10-25.** Continue preparing I4 for local review with
bounded effort. Single-digit-millisecond slowdowns do not justify prolonged
optimization; costs of hundreds or thousands of milliseconds warrant attention.
Report absolute elapsed/CPU times and complete-build totals alongside ratios.
The existing paired measurements imply +0.118 ms elapsed for Bombsweeper and
+0.619 ms for DPadHero3Proto per standalone link. Those costs may proceed to
review without another cycle solely to satisfy the strict percentage rule.

Use a short survey across the remaining object-project stages to look for
material costs; do not present it as the full formal timing gate. The design
and performance protocol describe the survey and its limits. This bounds
optimization effort without assigning an unrequested universal millisecond
allowance. Large-project and analysis/bundle requirements, exact ROMs, safety
checks and independent review remain in force. This is not I4 approval.

## 14. I5 timing effort and absolute costs

**Owner decision, 2026-10-25.** After the bounded analysis survey reported
producer costs up to about 15 ms on Metroid, with the first optimized complete
bundles roughly level or faster, the owner chose: "Document small costs; focus
on material regressions." Carry these measured small absolute costs into local
review instead of prolonging optimization solely to meet the strict
per-configuration timing ratio. Continue surveying the full inventory and
investigate material regressions. Report producer, consumer and complete bundle
costs separately; a faster consumer does not make a slower producer a speedup.

This is a bounded-effort decision for I5's measured costs, not a universal
millisecond allowance, a formal no-slowdown pass, or implementation approval.
Correctness, exact ROMs, the analysis memory gate and local independent review
remain required. The performance protocol and I5 validation record retain the
samples, absolute deltas, artifact sizes and remaining limitations.

## 15. I5 analysis corrections and publication

The I5 review exposed these consequences of the existing final-facts contract.
The owner approved these I5 corrections on 2026-10-25; they introduce no new
source-language feature. The citable stage approval is in `v2/README.md`.

| Case | v1 | v2 |
| --- | --- | --- |
| Numeric `.char` in a text listing | Omits the bytes, shifting later displayed addresses and `$` values | Shows actual bytes and final addresses |
| Label row before `.undef` and redefinition | Uses the final name's address | Uses that declaration statement's final address |
| Unused pure-binary `K .equ ^L` | Reports value 0 and includes `K = $0000` in the text symbol table | Reports `null` and omits K from the text symbol table; assembly still succeeds |
| Detected generation/allocation/I/O failure in a later sidecar | May already have replaced binary/earlier sidecars | Stages the entire file set; preserves previous destinations on generation failure, and rolls back a failed publication rename |

Literal-string `.char` grouping keeps v1's lowered `.db` grouping. String
substitution produces no address-computation references. Analysis accepts the 32/64-level alias controls and rejects the 65-level
control, as v1 does; an unused pure-binary bank constant remains
unknown and does not turn a successful assembly into an error. None of these
rules permits dropping references on a representational or resource failure.

Diagnostic listings remain the existing exception: unsuccessful, with no
emitted bytes or file offsets. Their addresses and extents are provisional;
they are not final layout facts. A successfully generated comparison mismatch
or audit finding retains its existing exit status and valid output artifacts.
File staging is not a crash-atomic filesystem transaction, and stdout cannot
be rolled back. If a filesystem failure also prevents rollback, report it and
retain the recoverable backup instead of claiming preservation.

The diagnostic-listing exception also requires successful destination planning.
After a syntax error, a symbolic data operand combined with FCEUX NL output
can prevent that planning; the previous listing is then preserved with
"could not collect assembly analysis" instead of publishing a diagnostic one.

The fail-closed requirement in section 6 remains the target. Inherited shared
collector failures still need propagation: some summary, text scope/source and
xref data-provenance allocations can silently lose metadata with exit 0.
Index, consumer and coverage failures preserve outputs but can exit 7–9 without
an explanatory diagnostic. File staging handles reported failures; it does not
detect these ignored collector failures. See the I5 validation record.
