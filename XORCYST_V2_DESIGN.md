# XORcyst v2: approved scope and design

Status: **D0 approved by the owner on 2026-10-22; D1 contracts and baselines
approved on 2026-10-23 after independent review.** The 2.0.0 implementation is complete.

Independent review found speculative scope and incompatible layout rules in
the initial design. This approved scope addresses demonstrated project needs
and states the compatibility decisions explicitly. Independent review and
owner approval are distinct; the approval record is in section 8.

**Gate:** D0, D1 and I1–I6 are approved and merged. I6 received owner approval
at `7105ea7` on 2026-10-26 after independent review. Section 7 preserves the
completed implementation sequence. Historical approval and review records
remain in [Git history](https://github.com/khansen/xorcyst/tree/9a273fa081524e3d55d608b3c9587ad55261aca4/design/v2); current release coordination is in
`RELEASE_CHECKLIST.md` in the source checkout.

D1 work and its approval record are in the
[contracts](XORCYST_V2_CONTRACTS.md) and
the frozen-input identities in `tests/projects/baselines.json`. D1 approval
supplements the historical D0 approval without waiving implementation gates.

## 1. Where the requirements come from

There are three kinds of evidence below. They are not interchangeable:

- **Owner decision:** explicitly requested direction or scope.
- **Project dependency:** behavior consumed by existing Nesrev and object/link
  game builds.
- **Reproduced defect:** evidence for fixing a particular failure, not proof
  that a project needs a broader language feature.

An implementation technique is a proposal, not a requirement. Each new feature
or compatibility change needs one of these sources, a concrete example, and
review. Permission to break formats does not require changing working source
semantics or every analysis field.

| ID | Requirement / boundary | Source and evidence |
| --- | --- | --- |
| R1 | Large-project builds must be no slower than v1, with and without analysis; improvement is the goal. | Owner instruction; frozen Metroid, Nesrev assembly profiles, and object-mode xasm/xlnk builds of bombsweeper, dpadhero2, ntxdev and nesdev/music projects. The Dropbox dpadhero build is non-blocking because it depends on precompiled v1 objects. Sections 5 and 6 define the fixtures and gate. |
| R2 | Preserve real projects' ROM bytes and the facts/outputs their workflows consume. | Nesrev's [bundle contract](https://github.com/khansen/nesrev/blob/260a50bd79a6e4a8a3f4104a3b928aee40d02154/ANALYSIS_BUNDLE_SPEC.md), readers/wrappers, and object/link projects inventoried in section 5. dpadhero2, bombsweeper, ntxdev and all 12 owner-nominated nesdev/music projects are required byte-identical ROM fixtures; dpadhero2 remains the explicitly non-waivable owner gate. |
| R3 | Bytes, listing values, instruction operands and references must agree. | Nesrev validates records against binary bytes. Repeated v1 failures are pinned by `test_right_shift_analysis_matches_emitted_bytes`, `test_current_pc_data_analysis_uses_each_operand_address`, and `test_binary_procedure_and_variable_addresses_match_linked_objects` in [test_inputs.py](tests/test_inputs.py). |
| R4 | Preserve output on errors and refuse incomplete analysis bundles. | Nesrev's manifest/hash/freshness checks; [dependency manifest contract](XASM_DEPENDENCY_MANIFEST_SPEC.md); `test_unresolved_analysis_values_preserve_all_outputs` in [test_inputs.py](tests/test_inputs.py). |
| R5 | Support one structured-listing schema, with an output identifier and coordinated consumer migration. | Owner decision. Listing v2 already fixes concrete byte/fill/mode errors; the [README listing contract](README) describes them. This does not require half-open ranges or a new analysis product. |
| R6 | Design full forward-string data support, including use-site character maps and final lengths. | Owner decision following reproduced binary/object disagreement. `.byte Text / After: .word After / Text .equ "ABC"` must give `41 42 43 03 00` at zero. No claim that Metroid needs forward strings. |
| R7 | Replace the object format where needed; no v1 object reader/writer is required in v2. | Owner decision. The v1 format constraints are recorded in [objdef.h](https://github.com/khansen/xorcyst/blob/86c1a43d1dfa237facf40d75085e9112e8617a13/objdef.h), [codegen.c](https://github.com/khansen/xorcyst/blob/86c1a43d1dfa237facf40d75085e9112e8617a13/codegen.c), and the [historical TODO](https://github.com/khansen/xorcyst/blob/86c1a43d1dfa237facf40d75085e9112e8617a13/TODO): count-minus-one storage cannot represent zero remainders, exports can retain unresolved aliases, and expression IDs have a fixed ceiling. |
| R8 | Retain the selected 32-bit arithmetic semantics on all evaluation paths. | Owner decision already implemented in [expr_int.h](expr_int.h); arithmetic/shift/export regression tests in [test_inputs.py](tests/test_inputs.py). |
| R9 | Do not reintroduce unbounded expansion, cycles, provisional-PC values or unsafe AST lifetimes while changing the internals. | Reproduced defects and existing tests: [scoped-offset cases](tests/test_expressions.py), [field-count/PC cases](tests/test_field_initializers.py), and [procedure analysis](tests/test_instruction_records.py). These justify safe ownership/evaluation, not a general equation solver. |

The v1 failures supporting R7/R9 are largely synthetic regression cases. They
are useful constraints on a replacement backend, but they do not automatically
make every TODO item a v2 release requirement. Additional work needs an actual
consumer, an explicit owner request, or a defect that the selected work exposes.

## 2. Smallest proposed architecture

Reuse the parser, opcode tables, existing arithmetic helpers, dependency
snapshots, and output staging. Share evaluated values and layout facts between
encoding and analysis, instead of maintaining independent evaluators with subtly
different rules. Direct binary and object linking should reuse that backend
where their existing placement models are equivalent. This is the proposed
means of satisfying R3/R7/R8, not authorization for a wholesale parser rewrite.

Use an owned post-expansion representation for only the facts those paths need:
statements and extents, operand expressions, bound definitions, locations, and
optional analysis provenance. Existing structures can be retained or adapted.
Analysis must not borrow pointers to nodes that lowering frees. Definitions
need stable identities internally; that alone is not a reason to expose a new
ID system in every output schema.

Evaluation distinguishes a known value, a deferred dependency, and an invalid
expression with a diagnostic. An error is not the integer zero. Preserve integer
versus string values and relocatable symbol references until resolved. Cache
results only in a context where bindings and layout are unchanged. Preserve
shared dependencies rather than cloning whole alias/offset trees repeatedly.
Cycle checks must identify actual dependencies, not reject every reference to
the same type. For example, `fa .byte / fb .byte[S::fb]` is acyclic;
`fa .byte[S::fb] / fb .byte` is not. Union offsets remain zero independently
of union size. These are existing language behaviors, not new capabilities.

Keep rich analysis opt-in. Plain assembly must not pay for collecting or
serializing unused trees, bindings, summaries, or source text. The existing
[records-v3 design](XASM_INSTRUCTION_RECORDS_V3_SPEC.md) documents an actual
large-project cost from repeated paths and the benefit of a shared file table;
preserve that improvement instead of introducing redundant metadata.

No table layout, header magic, generic extension mechanism, or public IR API is
frozen here. D1 should specify the minimum representation needed by the retained
operations and measured inputs within this approved scope.

## 3. Existing behavior to preserve, and bounded fixes

### 3.1 Source binding and type semantics

Retain v1.9.12 source binding unless a separately justified correction is
reviewed. In particular, withdraw the first draft's new assignment-generation
and `.undef` rules:

- `.byte I / I = 1 / I = 2` retains its existing forward-use result, `02`.
- Later uses and captured assignments retain their existing behavior.
- Local aliases retain definition-scope binding, already established in v1.
- Existing `.undef` lookup behavior is not changed merely to simplify an IR.
  Nesrev's committed `pair_redefined_label` fixture depends on the later table:
  its `LDA RawPointerTable,Y` must remain `B9 00 82`, not become `B9 0F 80` or
  an error. The original proposal would have broken a demonstrated dependency.
- `.ifdef` still checks what is defined at that source point.

Do not introduce a blanket declaration-captured type size. Preserve existing
capture of reassigned dimensions. One explicit exception is a field dimension
that still depends on an instance's PC, directly or through an alias: reject it
with a located error in both modes. This includes omitted outer arrays and
nested field counts, whose v1 binary/link layouts disagree. A constant that
already captured a valid fixed value is not such a dependency. Ordinary
storage-statement counts using `$` retain their existing rules. Section 3.5
records this bounded change; it is not a general type-system redesign. Other
inconsistent cases need a reproduction and design review before changing them.

Likewise, do not add forward union dimensions or cross-unit type definitions
for 2.0. `.extrn x : UserType` continues to use the importer's local type view.
Its byte/word/proc/etc. annotation is not a provider-kind assertion: constants,
strings and procedures imported `: byte`, and a byte variable imported `: word`,
remain legal. The provider supplies the actual value kind when evaluating an
expression. No new import syntax or type-matching protocol is required.
Preserve object mode's implicit external-label behavior and its warning:
dpadhero2's `game.asm` uses undeclared `reset` and relies on "undeclared;
assuming external label". D1 must pin both the warning and successful linking
with a fixture; pure binary still requires a definition. The same behavior
appears in Dropbox dpadhero's `common/timer.asm`, `main.asm` and `password.asm`;
these are additional source-level examples, not a dependency on its song objects.

### 3.2 Strings and exported constants

R6 requires forward and imported strings to resolve before their emitted size
is used. Capture the character map at the data use, not when the string later
resolves. Preserve numeric `.char` operands bypassing that map. Store the map
with a deferred `.char` use if the object needs it; do not add a map facility
to unrelated operands.

Preserve existing concatenation and comparison behavior. Two integer operands
to `+` add; two strings concatenate; mixed string/integer `+` uses signed
ASCII decimal for the integer, without leading plus or padding. Resolve operand
kinds before applying integer identities such as `x+0 == x`. Existing string
comparisons remain bytewise/lexicographic. This is current functionality, not a
new string language.

A string used as data must have a known value and length before placement.
A value already proved fixed during expansion/binding is usable; one that
still depends on later addresses or instruction relaxation is rejected with a
located error. Do not add a solver for address-dependent string lengths.
Check lengths before concatenation/allocation, including doubling chains whose
materialized strings grow despite a shared expression graph. D1 should reuse
existing resource controls where possible and specify a common bounded policy
for both output paths.

Objects need explicit string/blob lengths and correct exports; no NUL-terminated
wire field may truncate emitted data after character mapping. This does not
add raw-NUL literal syntax or new source encodings. The existing source-literal
NUL truncation is a separate input-validation defect to track, not a requirement
for a general binary-string language. D1 must document existing decoding and
supported escapes before changing their representation.

Resolve exported forward constant aliases before encoding when possible; keep
the necessary dependency otherwise. Never encode a constant name as a local
label index. This addresses an identified failure without changing import hints.

These intended changes need explicit migration fixtures, rather than a broad
claim of unchanged output: a forward string must move following labels by its
actual length, and a deferred `.char` string must receive its captured map.
With H-to-1/I-to-0, `.char Text, "HI"` and imported Text = "HI" changes v1's
`48 49 01 00` to `01 00 01 00`. Exported aliases that currently fail should
resolve to the same values as their locally defined equivalents. Both known
and forward `S .equ "A"` in `.byte S+0` must emit `41 30`; the forward linked
case currently drops the decimal zero and emits only `41`.

### 3.3 Storage, addresses, and placement

Objects must represent a zero **field remainder** directly; this is required
by valid existing initialized fields. It does not imply accepting `.dsb 0` or
`.T[0]` where v1 rejects them. Remove the proposed blanket zero-storage change.
Negative counts and invalid extents still fail independently of neighboring
positive storage. Preserve each statement's location when coalescing output.
Pin the zero-remainder case with a provider exporting N=2, an importer declaring
`nm .char[N] / fb .byte` in S, then `v .S {"AB", 7} / .byte $FF`. Both the
linked importer and a direct-binary control with local N=2 must emit
`41 42 07 FF`; v1 linking rejects the zero remainder.

Retain xlnk's per-data-label RAM allocation, zero-page/alignment/size priorities,
and first-fit `ram{}` placement. `.zeropage` and data alignment remain actual
constraints. Pure binary retains its source-ordered data addresses. These are
different placement inputs to the backend, not grounds for replacing either
with whole-section allocation.

For unit A's `big .byte[4] / small .byte` and unit B's `tiny .byte`, linked RAM
at $10 places small=$10, tiny=$11, big=$12. Pure binary's source order instead
gives small-big=4. Apart from the explicit exceptions in section 3.5, equality
is required with equivalent placement: matching RAM allocations, code origins,
bank assignments, file order and padding. A shared source file or code origin
alone is insufficient. Differences between separate RAM allocations cannot
cancel a supposed common section base.
Features supported in only one mode fall outside "equivalent placement":
expansion-time absolute `$`, `.org`, `^`, and imported or implicitly external
symbols are compared only in their supported contexts, not required to have
matching acceptance across modes.

Place RAM with known extents before selecting code widths, as the existing
linker does. Support the independent code-label difference `.dsb Tail-Head`
before `Head: nop / nop / Tail: rts` on both paths, giving `00 00 EA EA 60`.
Today only binary mode supports it. This bounded convergence does not require
a general cyclic-layout solver or address-dependent RAM allocation. A count
must not silently emit a different extent from the one used to assign
following addresses. Section 3.5 decides each known mode disagreement.

Keep `.org` pure-binary-only; do not add origin constraints to object files.
`link{origin}`, `bank{}`, padding and `copy{file}` keep their script semantics.
Numeric arguments must consume their complete token: the owner approved
rejecting v1's ignored suffixes and digitless prefixes on 2026-10-25. The
contract ledger records the examples and located-error/output-preservation rule.
Copy includes a raw file, not an assembly fragment. Keep captured input length
and identity for layout/publication checks. Do not add new CPU wrapping,
strict literal/range, zero-size RAM placement, or address-space policies as
part of the object-format change. Preserve existing range checks, including
the explicitly retained late word-instruction difference in section 3.5.

Keep unary `^Symbol`'s existing supported behavior: the owning/provider unit's
link-script bank, integer -1 when the unit has no bank, and no pure-binary
support. Without a bank, `.word ^Far, ^Far+1 / .dword ^Far` emits
`FF FF 00 00 FF FF FF FF`. Do not derive it from the output offset. Nesrev's
physical 16 KiB PRG page (`output_offset // 16384`) is a different concept.
The parity contract applies to supported equivalent contexts, not to a new
bank feature in xasm.

### 3.4 Instruction widths and provisional values

The first draft's all-wide starting point is withdrawn. Independent review showed
that it overflows every bank in frozen Metroid, including $10582 versus the
fixed bank's $10000 boundary. Those 3,000 symbolic zero-page instructions must
retain the widths v1 selects before layout.

Use v1 xlnk's repeat-until-nothing-shortens relaxation on both paths, retaining
its narrowing rule. Final range checks follow the policies in section 3.5,
including binary's retained warn-and-truncate behavior for late wide operands.
v1 xasm stops after two layout passes;
that difference cannot survive in a shared backend. Constants (including
resolved forward constants), placed RAM addresses, and imports resolving to
them receive their final widths before code relaxation. Only genuinely
layout-dependent operands need repeated layout work. Do not replace this with
the earlier greedy trial/no-retry proposal. D1 must specify the termination and
work bound, and test an adversarial cascade under the performance gate. Final
validation covers all operands (including wide ones), branches, data, counts
and bank/origin/padding limits; no output uses stale provisional addresses.

Retain these review counterexamples as contract requirements, not proof of a
new algorithm's implementation:

- RAM v at $10, code at $8000: 43 `lda v` instructions and `bne loop` retain
  43 copies of `A5 10`, then `D0 A8` (-88). No artificial -131 wide state.
- `lda T-3 / T: nop` at zero remains rejected on both paths.
- `lda Z / lda T-6 / T: nop`, Z=$10, at zero retains `A5 10 AD FF FF EA`
  on both paths under the current signed word-operand range policy.
- At $FA, `lda M / lda L / nop / L: rts`, M=$10, retains
  `A5 10 AD 00 01 EA 60` on both paths.
- At $F6, `lda L / lda M / M:` followed by four NOPs and `L: rts` converges
  on `A5 FE A5 FA EA EA EA EA 60`, the v1 linked result. The v1 binary
  `AD FF 00 A5 FB EA EA EA EA 60` changes deliberately; cascades in this
  class are the intended pure-binary change from sharing xlnk's loop.

Reject an expansion-time use of `$` when the final absolute PC is unresolved,
including uncertainty from instruction shortening, deferred extents or object
relocation. Apply the same rule through aliases and to conditionals, loops and
messages; report a located error rather than invent a provisional value. Thus
`lda Fwd / .if $ == 3` with later `Fwd .equ $10`, and object-mode
`nop / .if $ == 1`, are rejected instead of selecting v1's provisional branch.
`.message $` at those points is rejected too. This is a deliberate migration
change: v1 does not consistently reject them. Known final PCs remain usable,
and `.ifdef` keeps its source-time rule. Do not add deferred diagnostic events
or an object event table. Independent review found no use of `$` in expansion
directives or type bodies in Nesrev, bombsweeper, the GitHub dpadhero copy,
dpadhero2, ntxdev or the music projects; its scanner detected a planted positive
control. Retain fixtures for the changed cases and recheck required frozen inputs
in D1. Scan Dropbox dpadhero only if it is used for optional source coverage.

### 3.5 Decisions for known binary/link differences

This is the exception and convergence ledger for the I4 equality gate. Inputs
must use features supported on both paths and have equivalent placement as
defined in section 3.3. Within that common feature set, only the rows marked
**retain difference** are excluded; future exceptions require explicit review.

| Case | v1 binary / linked behavior | Approved v2 scope decision |
| --- | --- | --- |
| Relaxation cascade at $F6 above | Two passes / repeat until stable | **Converge:** both emit `A5 FE A5 FA EA EA EA EA 60`. |
| Late two-byte instruction operand outside -32768..65535 | Warns and truncates / rejects | **Retain difference:** preserve both outcomes; exclude acceptance/bytes from I4 equality. This is an instruction range policy, not `.word` data, which already warns on both paths. |
| PC-dependent omitted or nested field counts, including `Inner[3-$]` | Three-byte / seven-byte instance in the cited outer-count case | **Reject both** with a located error under section 3.1; do not choose an instance-PC convention. |
| `.dsb Tail-Head / Head: nop / nop / Tail: rts` | `00 00 EA EA 60` / layout error | **Converge:** both emit `00 00 EA EA 60` for this independent code extent. |
| Data-segment counts built from data-segment labels | Works in source-ordered binary / fails RAM layout at link | **Retain difference:** preserve binary support and the located link error when a RAM extent requires unplaced RAM addresses. Exclude these from I4 equality; no new RAM dependency solver. |
| `.byte Text / After: .word After / Text .equ "ABC"` at zero | Rejects / `41 42 43 01 00` with stale After | **Converge:** both emit `41 42 43 03 00`. |
| `.byte S+0 / S .equ "A"` | Error / `41` | **Converge:** both emit `41 30`, matching the known-string control. |
| Imported N=2, `nm .char[N] / fb .byte`, `v .S {"AB",7} / .byte $FF` | Local-constant control emits `41 42 07 FF` / zero-remainder link error | **Converge:** both emit `41 42 07 FF`. |
| Deferred `.char Text,"HI"`, Text="HI", H-to-1/I-to-0 map at the use | Deferred/imported text can bypass the map, giving `48 49 01 00` | **Converge:** forward and imported strings emit `01 00 01 00`, matching a known local string at the same use. |
| Exported forward constant aliases (§3.2) | Export can retain an unresolved definition instead of its value/dependency | **Converge:** an importing use gets the same value as its locally defined equivalent. D1 pins provider/consumer fixtures and link orders. |

Shared evaluation and encoding can retain these two explicit differences as
mode-specific validation/placement policies. They do not require separate
arithmetic or serialization implementations. All other known cases above have
a stated common value or rejection, rather than an unspecified parity promise.

The subsequent D1 clarification ledger in `XORCYST_V2_CONTRACTS.md` records the
owner's 2026-10-23 choices: reject `sizeof(label)` over unresolved string data
or imports, preserve forward local integer/address operands with an assembler
kind check, and reject explicit-address `.label`. The ledger also records the
correction when a string becomes known before the sizeof use. These remain
included in the completed D1 review and approval, separate from the original
D0 approval; implementation follows the staged gates.

## 4. Object and analysis contracts: minimum change

The new object format is permitted to be a clean break (R7). Specify only the
information required to encode existing supported instructions/data/storage,
forward strings, exports and locations. Use explicit lengths/counts, portable
fixed-width encodings and checked references. Preserve expression sharing
where it removes repeated unresolved-tree expansion. No general plugin system,
optional-section framework, public type-export system, or multiple object
versions is a requirement. Reject old objects with a rebuild diagnostic.

Validate malformed objects and allocation/resource failures before publishing
output. Normal objects retain source locations for diagnosable operands,
counts and definitions. Do not introduce a new stripping/provenance-profile
feature. Reuse existing debug/source information as appropriate; source text
can remain optional. Diagnostics identify an unavailable location honestly
rather than inventing one; semantic error categories need not have identical
assembler and linker prefixes.

Use the existing complete listing-v2 facts as the starting point for one
structured schema, then migrate Nesrev from listing v1. Retain current field
meanings and inclusive range conventions where they work. Withdraw the blanket
half-open conversion, new 64-bit JSON policy, and new public identity scheme.
Changing a field now requires a concrete consumer or correctness reason.
The structured listing and associated analysis outputs are pure-binary-only
in 2.0. Remove v1's section-relative object-mode structured listings and xref
exports, with a clear unsupported-mode error. Nesrev requires pure binary and
the reviewed game builds request no analysis. This is an explicit compatibility
removal, not a claim that v1 lacked those outputs. Keep object-mode text listing
only if the shared renderer supplies it without a separate evaluator or layout
path; it must label unplaced addresses as section-relative. No post-link
analysis system is required.

Do not change enum/scoped reference visibility just to make it theoretically
uniform. Preserve the existing semantic facts and filters used by consumers.
Additional reference categories need a demonstrated use. Existing stable file
and source/use identities must continue to disambiguate macro expansions;
internal lifetime improvements do not force new output joins.

Keep successful publication gated on complete requested facts, with safe output
path planning and the existing manifest committed last. Preserve fault-injection
coverage independently of natural source errors. Retain the documented refusal
when incomplete ROM layout prevents a safe destination plan; fixing that
limitation is not a requirement for a new transaction framework. Multi-file
renames are not globally atomic, and consumers still validate the bundle.

## 5. Existing projects and consumer surface

### 5.1 Object/link ROM fixtures

bombsweeper, dpadhero2, ntxdev and the 12 nesdev/music projects are required
byte-identical ROM fixtures; the Dropbox dpadhero build is non-blocking as
specified below.
These are not just examples of linker syntax. The owner's music projects add real
multi-unit sound-driver and asset workloads. Baseline and candidate must each
rebuild their own objects from the same source/assets and produce identical
complete ROM bytes (header, PRG, CHR and padding). Reusing v1 objects is neither
possible nor a valid test of the new format.

| Project / current build entry point | Evidence and baseline status |
| --- | --- |
| bombsweeper checkout, `.` | `ram{}`, raw header/CHR `copy{}`, `bank{}`, link origin $C000 and vector padding at $FFFA; `.extrn NMI, RESET, IRQ : word` confirms hints cannot enforce provider kind. Independent review built a v1.9.12 ROM of 24,592 bytes, MD5 `b33782b3b961b6fbb8b4aad3a37e3b7e`. |
| archived dpadhero checkout, `src/` | **Non-blocking fixture:** shared sources are present, and independent review reproduced the 262,160-byte ROM (MD5 `6374b615ea1f565c6e3556677a312318`) with v1.9.12, but only by reusing seven precompiled v1 song objects. Per owner direction, do not recover/convert these objects or add legacy-format support. Exclude this full-ROM build from I4, performance and release gates until rebuildable song sources are available. |
| dpadhero2 checkout, `src/` | Mandatory owner acceptance gate: the existing build must succeed and `build/dpadhero2.nes` must be byte-for-byte identical to v1.9.12. Eight 16 KiB PRG banks, imported `mixer_state`/`player_state`, extensive `.dataseg`, `.charmap` and `.char` exercise the object path. Independent review built the working-tree inputs, including the `game.asm` edit: 262,160 bytes, MD5 `841bfe4ef75d0e274ece167cfbd4b8ee`, matching the existing ROM and a repeat build. Preserve its implicit-external warning for `reset`. |
| ntxdev (Neotoxin) checkout, `.` | Owner-nominated `make` build of `ntxdev.nes`, using object-mode xasm and `ntxdev.s`. Eight 16 KiB PRG banks, RAM/zero-page allocation, vector padding, raw header/CHR copies, imported user types and records exercise the object/link path. Independent review built the working-tree inputs including `warp2d.h`: 262,160 bytes, MD5 `0ac03c2493892df178053ae553d8a6db`, matching the existing ROM. |
| nesdev checkout, `music/<project>/` | Owner-nominated object/link projects; their Makefiles and scripts exercise multi-unit sound drivers, RAM placement, bank/pad/copy, and generated song data. Independent review successfully built all 12 projects in isolated copies with v1.9.12. Freeze each actual build target, inputs and tools for byte-identical ROM checks and timings. |

The GitHub dpadhero copy is no longer the build fixture. Preserve its historical
reference ROM (262,160 bytes, MD5 `367256940188bcad697fd9577632a71e`), but do not
require the different Dropbox sources to reproduce that older artifact.
The Dropbox build's dependency on old song objects does not justify a v1
reader in v2 or a one-time conversion project. Source-only probes may be used
as optional coverage, without making this ROM build a prerequisite for D1 or
cutover. If rebuildable song sources become available, baseline capture must
force a clean link: the existing Makefile omits `data/*.o` dependencies.

The music inventory currently contains 12 project directories with Makefiles:
`bob-omb-battlefield`, `dire-dire-docks`, `double-dragon-ii-vs-f1`,
`dpadhero3proto`, `is-it-scary`, `live-and-learn`, `mega-jean-trooper`,
`merry-xmas-everybody`, `somewhere-only-we-know`, `there-must-be-an-angel`,
`welcome-to-paradise`, and `wishmaster`. Identify builds by directory and input
manifest and exact Makefile target, not by globbing ROM filenames: some Makefiles
share a PROGRAM name. In particular, dpadhero3proto builds `DPadHero3Proto.nes`;
the dated leftover `DPadHero3Proto20250221.nes` is not that build's product.

These paths identify the owner's current inputs, not a requirement for that
filesystem layout in CI. Before implementation, freeze reproducible copies
with source/asset/tool hashes and build commands. Include local modifications,
untracked project inputs and referenced shared sources; a git revision alone
is insufficient (dpadhero2 has a local `src/game.asm` edit, ntxdev has a local
`warp2d.h` edit, and the music tree has local projects and regenerated inputs).
Leave the original trees untouched; rebuild objects in isolated copies rather
than reuse existing build artifacts.
Explicitly select the baseline/candidate assembler and linker rather than
accidentally running project-local executables. Freeze the installed xm2nes and
xm2btn hashes as well as assembler/linker hashes. The review builds establish
feasibility; D1 still freezes manifests, commands and fresh v1.9.12 baselines.
For required fixtures, an unbuilt project is an outstanding gate, not a pass or
a reason to substitute synthetic probes. In particular,
dpadhero2 cannot be waived or have expected ROM bytes updated to accommodate
v2. No candidate build or ROM-equivalence result is claimed by this document.

### 5.2 Nesrev analysis consumers

Nesrev inspection baseline:
`260a50bd79a6e4a8a3f4104a3b928aee40d02154`, read through git objects only.
This inventory establishes dependencies; it does not claim they have migrated.

| Current output | Disposition for 2.0 |
| --- | --- |
| Text/JSON/NDJSON listing | Keep pure-binary outputs with one structured schema; remove object-mode structured listings. Object text listing is conditional on reuse as specified in section 4. No blanket range-convention rewrite. |
| Xref text/CSV/JSON; data, owner, instruction, local/anonymous options | Keep pure-binary facts, filters and deterministic order; port collection to shared values. Remove section-relative object-mode exports as specified in section 4. |
| Standalone and embedded instruction records | Keep byte/operand validation, memory access, additive terms, bindings, source trees and shared file table. |
| Xref summary | Keep rankings, filters, referrers and nearby-window output in its supported formats. |
| Index patterns | Keep registers, displacements, bounds, split-pair facts and source references. |
| Data consumers | Keep per-table extents and aggregated access facts. |
| Data coverage | Keep declared, covered and uncovered ranges and uncertainty flags. |
| Raw-address audit | Keep classification, configured ranges, severity and text/JSON reporting. |
| Binary comparison | Keep byte/slice comparison and source-mapped text/JSON diagnostics. |
| Dependency manifest | Keep input identities, negative lookups, producer identity, completeness and freshness checks; invalidate incompatible caches. |
| FCEUX ROM/RAM NL and mirroring | Keep the external format, PRG-page mapping and destination protection. |
| `--listing-version` / old selectable schemas | Remove only at coordinated cutover, per the owner's decision. |
| New xlnk analysis/debugger outputs | Not part of 2.0; no demonstrated Nesrev dependency. Existing binary/assembly linker output remains supported. |

Concrete migration/revalidation targets:

- Core readers: `scripts/analysis_bundle.py`, `data_directive_xref.py`,
  `instruction_records.py`, and their tests.
- Direct readers: `consumer_audit.py`, `orphan_opcode_scan.py`,
  `embedded_pointer_audit.py`, `static_analysis.py`, `used_by_xref_check.py`,
  `branch_literals.py`, `split_pointer_targets.py`, `ram_accesses.py`.
- Other extent/pointer readers: `embedded_pointer_targets.py`,
  `pointer_targets.py`, `pointer_table_body_check.py`,
  `data_extent_missing_scan.py`, `data_blob_dispositions_check.py`,
  `analysis_prefix_compare.py`, `refresh_inventory.sh`.
- Producers/wrappers: `project_pass_prep.sh`, `project_audit.sh`,
  `project_compare.sh`, `project_intake.sh`, `project_static_analysis.sh`,
  `project_verify.sh`, `project_ci.sh`, `verify.sh`, `new_project.sh`, and
  pass/process/review-packet callers.
- Contracts and fixtures: `ANALYSIS_BUNDLE_SPEC.md`,
  `NESREV_STRUCTURED_ANALYSIS_MIGRATION_PLAN.md`, relevant feature specs,
  Python/shell tests, and generated fixtures under `projects/*/fixtures`
  including dated nested directories. Preserve `pair_redefined_label`'s meaning.

The bundle requires `build.pure_binary == true`, separate instruction records,
listing `"1"` and xref `"2"` today. Updating a schema validator is not enough:
validate bytes, emitted values, ranges, joins, warning policy, cache identity
and output freshness. Do not turn the bundle into an object/link analysis
consumer without a project requirement. Finish the flag/reader inventory before
D1 schema approval; do not silently omit a product because one core reader
does not use it. Leave the separately deferred records-v4 proposal deferred.

## 6. Performance acceptance

**No slower than v1 on large projects, in each supported workflow configuration.
Ideally faster.** This is an owner requirement, not an aspirational benchmark.
The old v1 5% plus 20 ms allowance is not a v2 regression budget.

Freeze a fresh release baseline (initially v1.9.12), compiler/options, project
inputs and invocation profiles. Use frozen Metroid plus other large existing
Nesrev projects selected from actual usage and the required game/demo and music
fixtures in section 5.1; the precompiled-object-dependent dpadhero build is
excluded. Preserve the original sources read-only.
Use matched baseline/candidate builds, serial alternating repetitions, and
report CPU time and elapsed time per configuration. Instruction counts help
explain changes but do not replace timings. Record peak memory, artifact sizes
and downstream validation time so costs cannot be hidden in serialization or
shifted onto Nesrev. Stage-specific measurements and their remaining gaps are
recorded with each implementation; they are not a completed cutover gate.

At minimum measure:

- plain assembly with no listing or analysis;
- object-mode xasm and xlnk separately, and complete clean builds, for each
  required game and music fixture in section 5.1.
  Hold asset-converter inputs/versions
  fixed and report their cost separately; assembler/linker regressions must not
  be hidden by unchanged asset-generation work or a faster stage elsewhere;
- text listing and each supported structured-listing serialization;
- xref, including owner/data and embedded instruction configurations;
- separate instruction records with their dependency manifest;
- summary, index patterns, data consumers, coverage, compare and raw-address
  audit when used independently by current workflows;
- FCEUX NL on/off for the applicable profiles;
- complete actual bundle profiles: `ci-data-v1` (the default strict bundle),
  `ci-instructions-v1`, `inventory-instructions-v1`, `process-instructions-v1`,
  `maturity-instructions-v1`, `instructions-v1`, `data-listing-v1`, and
  `pass-prep-instructions-v1`. These names identify current command sets, not
  future schema IDs; additions discovered during the final inventory also count.

Compare equal useful work: retain the bytes and requested facts. For the listing
migration, validate that the new facts are correct and record changes in output
size; do not get a speedup by omitting required outputs or accepting incomplete
analysis. A win in plain assembly cannot offset a regression with xref or
listings, nor can a small synthetic fixture stand in for Metroid.

Investigate run-to-run noise with paired repetitions and controlled builds;
an inconclusive measurement is not proof of a pass or permission for a slowdown.
D1 must specify sampling and the decision rule before performance-sensitive
implementation. A reproducible regression in a required large-project mode
blocks cutover. No averaging across modes, inherited 5% tolerance, or automatic
exception for the new architecture. Any later relaxation of this requirement
needs an explicit owner decision.

**Memory tradeoff accepted for the current I4 implementation, 2026-10-25.**
These are mostly small NES programs. The owner accepts the measured additional
memory for now, provided its purpose is explicit and independently reviewed.
In the current focused standalone-link session, dpadhero2 peaks at 3,391,488
versus 2,637,824 bytes (+753,664, +28.6%); DPadHero3Proto peaks at 2,719,744
versus 2,080,768 bytes (+638,976, +30.7%). The latter is an accepted exception
to the earlier below-30% rule, not a measurement below that threshold. Continue
reporting each tool separately, including absolute bytes and ratios; complete
build peaks must not hide a tool's cost. This accepts the current measured
overhead, not unbounded growth or I4 implementation approval.

V1 walks compact object bytecode directly. V2 retains layout records, shared
expression values and dependency/cache state, definition/address identities,
and source spans for located link-time errors. That supports consistent
assembly/link evaluation and diagnostics, but does not make every current
representation choice necessary. Instrumented dpadhero2 storage after layout
is 1,116,430 requested bytes versus 546,440 for v1; layout and expression state
account for 727,456 of the v2 bytes. This allocation snapshot includes spare
capacity and is not an exact partition of peak RSS. Byte-only linking allocates
neither a complete ROM buffer nor full listing/xref fact arrays. Detailed
accounting is preserved in the historical I4 validation report; the current
[performance policy](doc/performance.md) summarizes the costs and rationale.

Independent I4 review found the peak occurs while translating the largest
object, with its raw wire buffer and translation workspace still live. The
post-layout allocation snapshot above does not measure that transient overlap.
The reviewed fixed-width objects were larger than v1's: about 5.9x for dpadhero2 and
11.4x for DPadHero3Proto. The validation record separates artifact sizes,
retained allocations and the reviewer's RSS trace. The reviewer measured lower
peak RSS with the current heap than with plain malloc, and no gain from mmap
input. No wire-format redesign or allocator expansion follows from this alone.
The 6–11x artifact expansion is not a language requirement, and D1 approval
of fixed-width records was not evidence that all emitted metadata was useful.
The follow-up writer drops unused reference/address spans and shares equivalent
leaves, while keeping literal and diagnostic use spans. Actual project objects
shrink 12.8% for dpadhero2 and 16.6% for DPadHero3Proto, still about 5.1x and
9.5x v1 respectively. Retaining fixed-width tables currently buys a simple
checked reader and native table access; it does not justify unused records.
A variable-length wire format would also need a decoding/translation design
to reduce peak RAM, rather than merely reduce disk size. Record this remaining
tradeoff and actual artifact sizes without treating expansion as a requirement.

Reviewers should assess whether those costs and the implementation complexity
are justified, and seek simple, measured improvements: avoid redundant copies
or state, size workspaces to actual use, and release construction-only storage
earlier where safe. Do not add disproportionate allocator or encoding machinery
merely to save a small absolute amount or bring this accepted case below 30%.
The existing custom heap is also subject to this review. Leaks, unsafe
lifetimes, pathological growth and unnecessary complexity remain findings.
The acceptance leaves ROM equality, timing, later analysis configurations and
independent review gates intact; see contract section 12 and the performance
protocol for the recorded scope.

**Bounded optimization effort, owner guidance 2026-10-25.** Interpret small-tool
ratios alongside absolute elapsed/CPU deltas and complete-build cost. The owner
does not want prolonged optimization of single-digit-millisecond increases;
hundreds or thousands of milliseconds merit investigation. The current small
linker deltas are about 0.12 ms (Bombsweeper) and 0.62 ms (DPadHero3Proto), and
may proceed to review with those costs explicit. Use a short survey to identify
material costs rather than repeated percentage-only tuning. Preserve the large
project/analysis requirements and report accumulated costs across a build.
Contract section 13 and the performance protocol record this clarification;
it does not waive correctness or claim a strict ratio pass.

**I5 owner decision, 2026-10-25.** The bounded analysis survey found producer
costs up to about 15 ms on Metroid, while the first optimized complete bundles
were roughly level or faster. The owner chose to document small absolute costs
for local review and focus further work on material regressions. Report
producer, consumer and complete bundle measurements separately, including
artifact sizes; do not present a survey or an accepted cost as a formal
no-slowdown pass. No universal timing allowance or I5 approval is implied.
Correctness, exact ROMs, the analysis memory gate and independent review stand.
Contract section 14 and the performance protocol record this decision.

Use focused stress fixtures only to protect against demonstrated pathologies:
shared alias graphs, dependent offsets, nested deferred arrays, and many
layout-dependent instructions in one section. Bound growth and measure it;
these are not a request for unrelated optimizer features. Pure assembly should
avoid work for unrequested analysis.

## 7. Review gates and sequence

The approval records cover D0 and D1. Implementation proceeds through the
following stages after the approved D1 revision is merged:

| Stage | Work | Evidence before advancing |
| --- | --- | --- |
| D0 | Approve this scope and its explicit decisions. | Complete: independent review followed by explicit owner approval on 2026-10-22. |
| D1 | Define minimum IR/layout/object/listing contracts and baseline fixtures. | Complete: independent semantic/wire review, frozen project baselines and performance protocol, followed by explicit owner approval on 2026-10-23; see contract section 8. |
| I1 | Shared values and owned dependencies. | Existing arithmetic, binding, cycle, lifetime and analysis regressions; no new language semantics hidden in the representation. |
| I2 | Layout/encoding using directly constructed fixtures. | RAM/fixed-width selection, banks, counts, address updates and range behavior independently checked; measured work bounds. |
| I3 | Existing frontend into an isolated direct-binary target. | Actual project bytes/diagnostics and plain-mode performance versus v1; no installed-tool replacement. |
| I4 | Object representation and linker integration. | Round trips, invalid inputs, exports/maps, zero field remainders, and binary/link equality over features supported on both paths under equivalent placement, subject to section 3.5's two retained differences. All required section 5.1 ROM builds are byte-identical, including dpadhero2; the precompiled-object-dependent dpadhero build is excluded. Object-assembly/link timings pass section 6. |
| I5 | Existing analysis products and Nesrev migration. | Every row in section 5 validated; real bundle profiles pass the no-slowdown gate. |
| I6 | Coordinated cutover and release preparation. | Final combined-tree CI, consumer validation, migration notes and removal of replaced paths. |

Open one implementation PR at a time after contract approval. Installed tools remain
v1 through I5; prototypes stay isolated. No permanent legacy-format mode is
required. Do not release until required producer and consumer changes are
merged and validated. Reuse normal Linux/macOS, strict-build, regression,
distcheck, sanitizer and output-failure coverage. Portable encodings need
cross-endian fixtures and host-width checks; adding new supported platforms
or mandatory emulated-platform infrastructure is not a separate v2 feature.

D1 must also pin the implicit-external warning and link fixture, decide when
the type-field `$` rejection fires (declaration, instantiation or `sizeof`),
and turn the deferred-map/exported-alias rows into exact contract fixtures.
Tool hashes and exact Makefile products are part of baseline capture, as above.

## 8. Scope ledger and approval record

| Earlier draft item | Current disposition and reason |
| --- | --- |
| New assignment generations / ambiguous-forward errors / `.undef` identity | Withdrawn. Breaks an actual Nesrev fixture without demonstrated benefit. Preserve existing binding. |
| Declaration-captured type sizes plus instance-dependent field dimensions | No blanket capture change. Reject field dimensions still dependent on an instance PC in both modes (section 3.1), instead of preserving incompatible layouts. |
| Blanket zero storage, stricter literals/ranges, new CPU-address limits | Withdrawn. Only zero field remainders are needed to repair the identified object limitation; other source changes need independent justification. |
| Forward union dimensions, cross-unit type exports, provider/import-kind matching | Not required. Preserve current syntax/restrictions and provider values with importer-local type hints. |
| All-wide initialization and new greedy relaxation | Withdrawn. Use existing xlnk selection and repeat-until-stable loop on both paths; deliberately change binary cascades, with explicit controls in section 3.4. |
| General forward `$` solver, deferred messages and object diagnostic events | Not required. Reject unresolved absolute PCs in expansion directives, including cases v1 accepted using provisional/section-relative values (section 3.4). |
| Known binary/link differences | Each decided in section 3.5: eight convergences/common rejections, two retained differences. Equality compares only features supported in both modes; one-mode features are outside equivalent placement (§3.3). No unspecified parity exceptions within that common feature set. |
| Object-mode structured listing/xref output | Remove the section-relative outputs; one structured schema targets pure binary. Object text listing may remain through the shared renderer, with no separate evaluator/layout path (section 4). |
| New linked-stage analysis, stripping profiles, generic extension tables | Deferred: no current Nesrev consumer. |
| Half-open listing conversion, 64-bit JSON redesign, new public reference IDs | Withdrawn as requirements. Preserve working conventions; only reviewed consumer/correctness changes belong in the migration. |
| New pure-binary bank operator behavior or `.org` in objects | Not required. Preserve supported v1 contexts and script semantics. |
| dpadhero's precompiled v1 song objects | No recovery/conversion work or legacy object reader, per owner direction. The full-ROM fixture is non-blocking until rebuildable sources are available; other required project gates remain unchanged. |
| New source string operators/encodings/raw-NUL literals | Not required for forward strings; retain current operators and separately track invalid-input handling. |
| Shared evaluation/ownership, sufficient object extents, forward strings, one listing schema, publication checks | Retained with the sources in section 1; specific implementations remain proposals. |

Independent review found the four scope decisions resolved: shared relaxation,
the mode-difference ledger, rejection of provisional-PC expansion, and real
project gates. Its final clarifications are recorded here: equality covers
common supported features and narrowing does not override the mode-specific
final range policy. The supplied dpadhero tree still depends on precompiled v1
song objects; the owner excluded their recovery/conversion from this work, so
that full-ROM fixture is non-blocking. D1 starts with the required source-built
project baselines, not legacy-object migration.

Approval record: **D0 approved by Kent Hansen on 2026-10-22**, following
independent review and the final clarification that all 12 music projects
are required. In response to the request to approve D0 and authorize D1
baseline freezing and contract design, the owner stated: "It's approved. You
can amend these changes and force-update the PR branch."

This approval applies to the scope recorded in this revision, including the
no-slowdown gate and the exclusion of dpadhero's precompiled-object-dependent
ROM build. It authorizes D1, not implementation. D1 and implementation have
not begun as of this approval-record update; implementation requires separate
review and approval of the contracts.
