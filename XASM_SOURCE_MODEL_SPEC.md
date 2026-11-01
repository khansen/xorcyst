# xasm lossless source model

Status: draft for independent review, 2026-11-01. Specification only; no public
source-model interface is implemented by this branch. Baseline: XORcyst 2.0.0
and repository commit `51dd295b89073b7858e4c6cf9a19776af7e9bbc7`.

## 1. Purpose and boundary

Let tools inspect and edit assembly using the assembler's complete interpretation
instead of reconstructing bindings, expressions or layout with text processing.
The model must also preserve the exact authored source. These are independent
requirements: a byte-perfect copy is not proof of semantic coverage, and matching
machine code is not proof of a lossless source round trip.

The invariant is:

```
source bytes -> saved model -> no-op processing -> reconstructed source bytes
                  original source unavailable
```

The final bytes must equal the input byte-for-byte. Reconstruction may read only
the saved model, not the original source, another checkout, Git, a listing's
selected excerpts, or a source backup. Comments, blank lines, tabs, capitalization,
literal spelling, delimiters and line-ending bytes are part of the contract.

xasm owns input snapshots, lexical units, syntax, scopes, bindings, evaluation,
layout, emitted operand ranges and diagnostics. A consumer may index, paginate,
present, search text fields, and make edits using these records. It must not
reparse assembly or evaluate expressions to fill a missing production fact.
Independent test evaluators are useful cross-checks, not a production fallback.

The consuming service owns edit transactions, stale-preimage checks, document
ownership, generations, persistence, access permissions, review and Git. xasm
does not gain those services or an agent-facing whole-source export endpoint.
The lossless model necessarily contains source; restricting who receives it is
the service's responsibility. Hardware identities and runtime reachability remain
evidence questions. Complete syntax and use lists do not prove semantic names.

The local spike demonstrated exact reconstruction, complete reference inventory
and segment-aware layout on a 23-source corpus. A downstream structured-only
workflow completed a real naming/comment pass with a disclosed operator hint.
Those results justify production design; they do not certify this interface or
show comparative quality or token savings. Public tests and documentation must
be project-neutral and require no private ROMs or local prototype directories.

## 2. Scope of the first profile

The initial capability profile is `asm-edit-v1`, for pure-binary 6502 assembly.
Its limits apply when the model is requested, not to ordinary xasm assembly.
It types the following constructs, including literal-only and unreferenced cases:

| Construct | Required information |
| --- | --- |
| Global, `@@` local and anonymous labels | Every declaration; native scope and symbol identity; every reference and its binding |
| `.EQU` | Declaration, complete expression, dependencies and resolved value; immutable single declaration per symbol |
| 6502 instructions | Parsed form, explicit `.W`, complete operand expression, final opcode/mode, access facts and emitted bytes |
| `.DB`, `.BYTE`, `.DW` | Original directive spelling, statement boundaries, every operand, native operand ranges and bytes |
| `.ORG` | Expression/value and segment transition, including repeated CPU origins |
| `.END` | Explicit syntax record even though the current parser discards it; no implied emission |
| Trivia and punctuation | Exhaustive byte coverage, including comments, whitespace and newline spelling |

Integer expressions include nested arithmetic, bit operations, comparisons,
projections, symbol/local/anonymous references and the program counter. Literal
strings in byte data are included. The expression vocabulary in §6 is exhaustive
for this profile; other value kinds or operators cannot be hidden in a generic
successful expression node. String-valued EQUs, string arithmetic and word-string
expansion are outside this initial profile even if ordinary assembly accepts them.

Includes, binary includes, character maps, macros, conditionals, loops, assignments
and redefinition, `.DEFINE`/`.UNDEF`, structures, procedures, enums, reserve/fill
directives, explicit code/data segment switches, imports/exports and object/link
models are later profiles. Command-line symbol definitions are also outside v1;
otherwise a symbol could lack the source declaration the editing contract needs.
Other harmless CLI options retain their ordinary meaning. Any option that creates
unmodelled semantic inputs must refuse certification rather than be ignored.

The lexer/parser must inventory actual constructs before lowering discards them.
A syntactically valid but unsupported construct returns
`SOURCE_MODEL_UNSUPPORTED_CONSTRUCT`, identifying its unit/span and capability.
There is no successful partial profile. Inactive branches are not a loophole.
Build an independent expected construct/reference inventory from the parsed tree
and recognized lexer constructs, not from the records the exporter chose to emit.

V1 also refuses duplicate named declarations, identical or conflicting, with
`SOURCE_MODEL_DUPLICATE_DECLARATION` and all declaration sites. Use the producer's
symbol namespace and scope IDs, not a consumer's global-label scan. Labels and
EQUs share the native namespace. Repeated local names in different scopes and
distinct anonymous labels are valid. Ordinary xasm's redeclaration behavior is
unchanged. Multi-declaration symbols require an explicit later model before a
rename can safely address them.

Validate profile coverage against a producer-generated inventory of all configured
private corpus inputs as an optional integration check. Public coverage comes
from neutral fixtures. Neither a fixed historical construct count nor a corpus's
successful assembly replaces complete coverage checks for each new invocation.

## 3. Public invocation and compatibility decision

Proposed interface:

```sh
xasm --pure-binary --source-model=model.json \
  --source-model-profile=asm-edit-v1 \
  --source-model-diagnostics=failed-model.json input.asm -o output.bin
```

`--source-model` alone selects `asm-edit-v1`. It enables its own required capture,
source snapshots and native layout metadata; it must not depend on requesting
instruction records, debug output or any other analysis product. Xref filters
must not remove declarations or uses from the model. The diagnostics path is
optional and requires `--source-model`. Empty/colliding paths, unknown
profiles, unsupported CLI combinations and object-mode requests are usage errors.
There is no environment-variable API or hidden `XASM_SOURCE_PROBE_*` prerequisite.

The model is one UTF-8 JSON document with schema `xasm-source-model` and integer
version `1`. It is self-contained for source reconstruction and static queries.
An emitted binary remains an ordinary separate output; the model embeds its bytes
once for range validation and includes its digest. A standalone dependency
manifest is optional: the model carries equivalent consumed-input evidence itself.

Existing listing, xref, instruction, audit, comparison and debugger outputs remain
useful products. They do not become inputs needed to interpret this model. Shared
native facts should feed both paths; do not build a second evaluator or retain
duplicate semantic engines merely to reproduce a historical wire format.

### Compatibility ledger and release rule

The owner permits breaking changes. **Any intentional public breaking change
requires xasm 3.0.0**, a migration document and coordinated consumer updates.
This includes analysis schema/meaning, accepted source, CLI/defaults, failure
behavior and object compatibility, even when final binary bytes are unchanged.
Schema versions and the executable release number are independent.

| Proposed behavior | Existing contract affected | Decision in this draft |
| --- | --- | --- |
| New optional model and diagnostic products | None for an invocation without the new options | Additive; does not alone require v3 |
| Strict single-declaration/construct profile | Only the explicitly requested new model | Additive; do not reject ordinary assembly globally |
| Source identity and byte-offset spans in the new model | No existing schema field is reinterpreted | New schema; not instruction-record schema 4 by implication |
| New native per-operand ranges | New model; existing data-reference meaning remains | Additive facts |
| Data-row grouping | Spike capture disables adjacent-data grouping | Production must preserve existing listing grouping, including combined invocations, using separate statement identity; otherwise declare this a v3 change |
| Replacement/removal of existing exports, changed field meanings or global declaration rules | Existing consumers or valid sources | Not proposed here; requires an explicit spec amendment and v3 |

There is no mandate to stay on 2.x. Choose a simpler v3 contract if implementation
reveals a real conflict; do not quietly change an old format or add compatibility
switches to avoid a major release. This draft identifies no intentional existing
public break. Final release numbering waits for the reviewed compatibility ledger
and differential results. Prototype sidecars are private and need no compatibility
adapter. The branch is a local specification proposal, not release approval.

## 4. Acquisition and capture inside normal assembly

Reuse [consumed-input snapshots](XASM_DEPENDENCY_MANIFEST_SPEC.md): capture exact
bytes at the real input-open boundary, and parse those same immutable bytes.
Capture before case folding, lexer buffer mutation, string decoding or discarded
comments. Record byte offsets directly; columns are display aids, not edit keys.
The capture must include bytes read outside the normal lexer match callback.

The parsed representation is recorded before pruning, substitution or folding.
Resolution attaches bindings to occurrences using the same namespace/scope and
resolution result as assembly. Lowering preserves syntax identities through
replacement and merging. Final layout/emission attaches values and byte ranges.
No extra assembly, source parser or post-hoc name-based rebinding is allowed.
Capture must not change reference counts, unused warnings, evaluation order,
diagnostics or output bytes. Forward fixups must attach their actual resolved
declaration; a final-table lookup alone is not sufficient evidence.

The lossless file snapshot covers every byte even when parsing fails. A failed
parse may leave unclassified intervals, explicitly marked in an uncertified
diagnostic model. A certified model may not use that escape hatch. `.END` follows
the real grammar; it does not license ignoring an arbitrary trailing suffix.

For later profiles, includes inside a false conditional are still opened during
parsing, and both branches must satisfy the grammar. Preserve parsed inactive AST
nodes before pruning; attach activity and enclosing conditional/expansion IDs
afterward. Consumed includes remain dependencies even when their branch is later
inactive. Do not invent opaque valid inactive source. These rules constrain later
extensions; v1 refuses those constructs instead of implementing them prematurely.

## 5. Envelope, bytes, spans and identities

Every certified table is required, even when empty; diagnostic nulls are specified
in §8. Records reference table IDs rather than
duplicating full expression trees or source text. Counts and foreign keys must be
validated explicitly, including under optimized builds; integrity cannot depend
on Python/C debug assertions. Unknown schema versions, enum values or required
capabilities refuse. A published machine-readable schema and examples must land
with implementation; the field contract below is the review authority.

| Top-level field | Meaning |
| --- | --- |
| `schema`, `version`, `profile`, `target` | `xasm-source-model`, `1`, `asm-edit-v1`, `mos6502` |
| `status` | `certified` or `diagnostic`; only the former is usable for semantic edits |
| `identity` | Input identity, producer digest, schema/profile IDs and model identity, as below |
| `invocation`, `dependencies` | Exact diagnostic command/cwd plus consumed file hashes, sizes, roles and missing probes |
| `files`, `units`, `syntax`, `scopes`, `declarations` | Source bytes, lexical partition, parsed nodes and native names/scopes |
| `expressions`, `references`, `evaluations` | Full expression structure, per-occurrence bindings and producer evaluations |
| `segments`, `locations`, `instructions`, `data`, `emissions` | Final assembly facts and source joins |
| `binary` | Length, SHA-256 and base64 payload; null in a diagnostic model |
| `coverage`, `diagnostics` | Expected/actual inventories and explicit failures or warnings |

Files have `id`, logical `path`, `byte_length`, `sha256`, and `bytes_base64`.
V1 has one root source, addressed by its basename in a logical root. Absolute
lookup paths live in the dependency/invocation evidence, not record keys.
Bytes are stored once per file. A unit has `id`, `file_id`, `byte_start`,
`byte_end_exclusive`, `kind` and optional `syntax_id`; its text is that slice of
the stored bytes. Unit kinds are `token`, `comment`, `whitespace`, `newline`,
and diagnostic-only `unclassified`. Token records also carry the lexer token
kind, using published symbolic names, never generated lexer rule numbers.

For every file, ordered nonempty units partition `[0, byte_length)` exactly.
No gaps or overlaps are permitted; an empty file has no units. A syntax span may
contain many units; a reference's spelling span can be a subspan of a token.
All spans are file ID plus half-open **byte** offsets. Optional line/column
coordinates are one-based byte coordinates with tabs counting as one byte;
they never control mutation. Base64 preserves arbitrary bytes independently of
JSON Unicode. Invalid source bytes may cause normal parse failure while still
being reconstructible from a diagnostic model. No newline or Unicode normalization.

The input identity hashes source logical path/content, effective semantic options
and consumed/missing semantic inputs. For v1's single-source profile, reject any
additional semantic dependency lacking a defined logical identity; do not omit
it. Comparison-only files are recorded separately and do not change source record
identity. The model identity additionally binds the executable digest and the
schema/profile IDs. Normalization preserves option order when it has meaning;
output filenames, timestamps, scratch-root prefixes and executable installation
paths are excluded. The implementation must publish the canonical serialization
and fixed hash vectors; do not use unspecified platform JSON serialization.

IDs are strings unique within a model: table prefix and deterministic ordinal in
source/preorder order; derived rows use their syntax ID and operand/emission
ordinal. Repeated builds with the same input and producer identities have the
same record IDs, regardless of output paths or checkout root. The exact running
producer's digest matters; a version string alone is insufficient. Stored facts
are not silently re-certified by newer readers. Consumers record their schema/
validator identity separately from input identity and refuse unsupported versions.

IDs need not survive a source edit, an assembler change or a profile revision.
Cross-edit correspondence comes from consumer edit provenance or an explicit
alignment operation. A same-address label, equal value or same-spelled local is
never an identity proof. No hash claims authorization or cryptographic attestation.

## 6. Syntax, symbols, expressions and uses

Syntax records have `id`, `kind`, `span`, `parent_id`, ordered `children`, and
`scope_id`. Published kinds include declaration, instruction, data, org, end and
expression forms. Same-line labels and instructions are separate nodes. Exact
spellings are retrieved through spans; normalized mnemonics/directives are
additional fields. Parentheses, commas, colons and mnemonic suffixes remain in
units even when the semantic tree has no separate node for each delimiter.

Scopes have `id`, `kind`, `parent_id`, `anchor_declaration_id` and source extent.
V1 supplies root and native global-label scopes, including the pre-first-label
region. Declarations have `id`, `syntax_id`, `name_span`, native `symbol_id`,
`name`, `kind`, `scope_id`, `lexical_owner_id`, `expression_id` and `location_id`
(nullable when inapplicable). Kinds distinguish global/local/anonymous labels
and EQU. The producer supplies scoped identity for locals; the service must not
derive it by rescanning surrounding global labels. Anonymous declarations have
distinct IDs even though their spellings repeat.

`lexical_owner_id` means source containment, not a proven routine boundary or
exclusive control-flow ownership. Facts must include unreferenced declarations,
alias labels sharing an address and declarations with no emitted bytes. Comments
have lexical spans/order, not assembler-certified semantic owners. Header/comment
association belongs to a separately versioned consumer policy shared with its
documentation gates; it must not change bindings or source coverage.

Expressions have `id`, `syntax_id`, `span`, `kind`, ordered `children`, and the
kind's payload. V1 kinds/payloads are:

| Kind | Payload |
| --- | --- |
| `integer` | Parsed signed 32-bit value; original literal remains in source units |
| `string` | Decoded bytes, for supported byte-data literals |
| `symbol`, `local`, `anonymous_forward`, `anonymous_backward` | Exactly one `reference_id` |
| `pc` | Evaluation contexts identify the native PC; never substituted by a consumer |
| `unary` | `complement`, `not`, `negate`, `low`, `high`; one child |
| `binary` | `add`, `subtract`, `multiply`, `divide`, `remainder`, `shift_left`, `shift_right`, `bit_and`, `bit_or`, `bit_xor`, `less`, `greater`, `equal`, `not_equal`, `less_equal`, `greater_equal`; two children |

Unsupported operators such as bank expressions or `sizeof`, and unsupported
value combinations, refuse this profile. Arithmetic and diagnostics follow the
native assembler's reviewed semantics, including overflow, shifts and truncation;
this schema does not authorize a new evaluator. Pre-fold trees remain visible
even when a whole expression evaluates to a constant.

Each reference occurrence has `id`, `expression_id`, `span`, `spelling`,
`context_syntax_id`, `scope_id`, `declaration_id`, and `diagnostic_id`.
Exactly one of the last two is non-null. A certified model has no diagnosed
reference. Count every symbol/local/anonymous leaf independently from the parsed
tree, excluding declaration names, and compare exact occurrence sets. This
includes nested masks/strides, data pointers and EQU-to-EQU expressions; counting
only additive terms or only references already exported is invalid.

Bindings must agree with the native declaration kind, scoped name and selected
definition. Anonymous references must identify the actual native forward/backward
binding. Capture resolution from the assembly path, including forward-resolution
events; the spike's post-pass symbol-table lookup is not the production contract.
The binding used for evaluation and the exported binding must be the same.

Evaluations have `id`, `expression_id`, `context_syntax_id`, optional
`operand_index`, `pc_context`, `value`, and `reference_values`. Values are tagged
`integer` or `bytes`; each reference-value entry identifies its reference and
declaration. Export native resolved EQU values and complete operand/ORG evaluations.
When one source expression has multiple native evaluation contexts, distinguish
them; do not overwrite one value with another. Evaluating unused declarations for
analysis must not change use counts; an unavailable value blocks certification
with a located diagnostic rather than being guessed.

Direct syntactic uses and transitive dependencies are different. For
`Distance .EQU Right-Left` followed by `LDX #Distance`, the instruction directly
uses `Distance`; its dependency graph reaches `Right` and `Left` through that
EQU. Do not manufacture direct instruction occurrences of those labels. Multiple
source occurrences remain distinct even at one output offset. Existing xref
duplicates or misplaced columns are not imported as source-model provenance.
Every reference span must match that reference's own expression leaf.

## 7. Instructions, data and layout

An instruction has `syntax_id`, `location_id`, parsed mnemonic/addressing form,
`forced_absolute`, final opcode and addressing mode, ordered operand IDs,
`emission_ids`, `memory_access` and `control_flow`. Operand records link the full
expression, native evaluation and native encoded byte range. Implied/accumulator
forms have no fictitious expression. A relative branch reports both the resolved
target and encoded displacement. Final zero-page narrowing and `.W` are explicit.

Use the producer's [memory-access contract](XASM_INSTRUCTION_RECORDS_V2_SPEC.md):
report data `read`, `write`, `read_modify_write` or none, and pointer-byte reads
separately. `CMP`, `BIT` and RMW instructions must retain their actual classes.
`STA Base+Field,X` is a write even without a unique structural base; every term
stays visible in that write context. Immediate arithmetic is not automatically
an address-taken use. Indirect destinations remain unknown unless the producer
has direct evidence; numeric equality does not select a declaration or bank.
Control flow distinguishes direct call, jump, branch, return, trap and none;
indirect transfers identify their pointer expression and unknown destination.
Implicit stack, device DMA and runtime bank switching are not analysed by v1.

A data statement has `syntax_id`, `location_id`, normalized element width,
original directive token, ordered operands and `emission_ids`. Each operand has
its index, expression/evaluation IDs, native element count, and native emission
ranges. Literal operands, empty strings and unused tables are included. For byte
strings, the range covers the actual expanded bytes. Ranges come from the backend
that emits them, not a consumer's width/string-length calculation. Instruction
operand ranges likewise exclude the opcode and describe actual encoded bytes.
Source statement/operand identities must survive any backend merging; `--debug`
must not change them. The model is not a dump of grouped listing rows.

Segments have `id`, `start_syntax_id`, address space and origin. Each `.ORG`
creates a distinct segment even when its origin repeats. Locations have
`syntax_id`, `segment_id`, `cpu_address` and optional `output_offset`.
Every label, instruction and data statement has exactly one location in the
certified v1 model. EQU values are not label locations. Non-emitting declarations
may denote an output position, but have no fictitious emitted byte.

Address keys are `(segment_id, cpu_address)`. Segment IDs express assembly layout,
not inferred cartridge bank numbers. An integer pointer without segment evidence
has an ambiguous destination; callers must supply context or see all candidates.
Output offsets and CPU addresses are separate coordinates. Raw PRG page arithmetic
does not establish runtime bank identity.

Emission rows have `id`, owning syntax/operand ID, `segment_id`, starting CPU
address and `[output_start, output_end_exclusive)`. They slice the saved binary
bytes. Statement ranges equal their operand/opcode pieces; aliases may share a
location but never duplicate byte ownership. Sorted positive-length top-level
emissions partition the whole binary exactly. If the backend emits padding/fill,
it requires a typed native row with its cause, value and extent; it cannot become
an unexplained gap. No consumer calculates bytes to repair a missing range.

Common fields avoid game/mapper terminology. Target-specific opcode, addressing
and memory semantics remain a named `mos6502` vocabulary. A future assembler for
another CPU must produce its own authoritative bindings and layout. This release
does not implement another platform or promise cross-assembler ID equivalence.

## 8. Certification, failure and publication

Before writing a certified model, verify all of the following using independently
enumerated expected sets, not counts alone:

1. Captured inputs are consistent; every source byte belongs to exactly one unit.
2. Every parsed/lexed construct is typed or produces a capability diagnostic.
3. All declarations/scopes are present, and duplicate symbols are refused.
4. Every reference occurrence has exactly one native binding; no extras, missing
   leaves or diagnosed references; kinds, names, scopes and spans agree.
5. Every instruction/data/label joins to exactly its own syntax and layout record.
6. Every supported operand/EQU has its native expression, evaluation and ranges;
   reference identity is checked independently of value equality.
7. Emissions cover the binary once, match its bytes, and respect segment identity.
8. Foreign keys, bounds, ordering, declared lengths, hashes and profile agree.

Warnings preserve the normal warning policy. Certification cannot turn a failed
assembly into success. Exit 0 means assembly and all requested success products
completed; exit 1 is an assembly or source-model/profile error; exit 2 is invalid
CLI usage; exit 3 is input-snapshot, model collection/serialization or publication
failure. Existing unrelated comparison/audit exits retain their documented meaning.
Internal capture failures are distinct from a user's invalid source: use
`SOURCE_MODEL_INTERNAL_ERROR`, not a misleading unresolved-symbol diagnostic.

Located model diagnostics have `code`, `severity`, `message`, optional `span`,
related spans and optional capability. Required codes include
`SOURCE_MODEL_UNSUPPORTED_CONSTRUCT`, `SOURCE_MODEL_DUPLICATE_DECLARATION`,
`SOURCE_MODEL_UNBOUND_REFERENCE`, `SOURCE_MODEL_VALUE_UNAVAILABLE`,
`SOURCE_MODEL_INCOMPLETE`, `SOURCE_MODEL_INPUT_CHANGED`,
`SOURCE_MODEL_RESOURCE_LIMIT`, `SOURCE_MODEL_IO_ERROR`, and
`SOURCE_MODEL_INTERNAL_ERROR`. They contain no shell commands or opaque traceback
in place of the error type. Normal source diagnostics remain available too.

When requested and capture is available, an unsuccessful assembly/profile check
can publish the separate diagnostic model: `status=diagnostic`, complete captured
bytes, syntax/lexical coverage and located errors. Final `binary`, `segments`,
`locations`, `evaluations`, `instructions`, `data` and `emissions` must be null
(rather than stale successful tables). Parsed instruction/data syntax remains in
`syntax`; partial bindings are explicitly non-certified. It is for candidate
repair, never publication, value queries or semantic rename. If acquisition or
allocation fails, do not claim a complete diagnostic capture. A successful run
does not read an old diagnostics file; consumers bind all files to the current
invocation result and model identity.

Plan all output destinations before writing any. Include the new products in
the existing collision and input-reservation checks, including symlink/hardlink
aliases. Stage binary and successful analysis products; any model failure prevents
their successful publication and preserves the previous output set under the
existing rollback contract. The diagnostic path is separate and explicitly
non-certified. Revalidate consumed/missing inputs around staging and publication.
Atomic rename is not a promise of multi-file crash durability: require a successful
invocation and matching identities/digests; stale files alone are not evidence.

All allocations and table/range arithmetic are checked. Resource limits must be
documented and refuse with a named diagnostic; never truncate tables or return
success after dropping records. Do not ship probe `exit(90..108)` paths or unchecked
debug-only integrity assertions as the production failure model.

## 9. Representation and cost

Start with one JSON document: a shared file-byte table, normalized record tables,
IDs and ranges. Avoid hex per lexer fragment, repeated file paths, copied
expression subtrees and source text on every fact. Persisted source bytes are
required; storing an extra textual source backup is not. SQLite, compression and
a binary format are not prerequisites. Measure before choosing one.

Report capture/assembly time, serialization, consumer validation and reconstruction
separately, plus full service rebuild/amend time as a downstream integration
measurement. Include elapsed/CPU time, peak RSS, artifact bytes and temporary-disk
peak with matched builds and the exact record counts. Follow
[development strategy](DEVELOPMENT_STRATEGY.md) and
[performance policy](doc/performance.md); no new regression allowance is granted
by this spec. Profile-enabled work has no comparable old complete product: show
its absolute costs and the incremental cost over equivalent ordinary assembly.
Measure unchanged ordinary modes separately; avoid paying capture costs when it
is disabled. Report both large sources and small fixtures, and retained versus
temporary storage. No blanket speedup or two-second full-amend promise is made.

## 10. Acceptance and independent review

Tests below are requirements, not claims about this draft. Use neutral, hand-checked
fixtures in the normal suite; private corpus results are additional evidence.
Each completeness guard needs a failing-direction case, with valid controls that
prove the setup exercised the intended path.

| Surface | Required proof and failing direction |
| --- | --- |
| Lossless bytes | Delete/make unavailable source, checkout copies and Git access before reconstructing; tabs, mixed case, CRLF/LF, UTF-8/comment bytes, blanks, missing final newline, empty file and parse errors. Corrupt/drop/reorder a unit or byte and require a named refusal. |
| Coverage | `.BYTE`, `.END`, anonymous labels, `.W`, same-line constructs and all supported expressions. Unsupported directive/operator in otherwise valid input blocks certification; missing entire record families cannot pass with zero counts. |
| Bindings | Repeated `@@loop` scopes, forward EQUs, nearest forward/backward anonymous labels, `.DW Entry`, `Alias .EQU Cell+1`, `#(MaskA|MaskB)`, `Base+(Stride*2),X`; remove/duplicate/redirect one leaf. Redirecting to a same-valued wrong declaration must fail. |
| Duplicate symbols | Identical/conflicting EQUs, label/EQU collision, unreferenced duplicates; all locations returned. Same local spelling in distinct native scopes succeeds. |
| Expressions and bytes | Hand-computed integers, projections, PC-dependent operands, strings and truncation; independently compare values and bytes. Corrupt a value plus its bytes together so a simple byte comparison cannot hide the bad binding/expression. |
| Native operand ranges | Adjacent data directives, multi-operand rows, literal-only data and strings, `.DW`, empty string and branch displacement; change one range or omit one operand. Debug and analysis-option combinations keep identical identities/ranges. |
| Access classes | CMP, BIT, RMW, indirect pointer reads, `STA Base+Field,X`, immediate label differences and aliases. Missing structural base must not erase read/write kind. |
| Segments | Two `.ORG $8000` segments with same-address labels, same-segment aliases and cross-segment symbolic references. Swapping/omitting a segment is detected; address-only queries cannot pick a bank. |
| Identity | Rebuild in another checkout/output directory with the same producer and inputs; IDs agree. Edit one source byte or effective option and input identity changes. Change producer/schema and model identity changes. |
| Capture independence | Model option alone succeeds; disabling old listing/xref/record outputs cannot empty it. Exporting must not change existing warnings/use counts or binaries. |
| Repair and publication | Success -> syntax error -> repair, each in fresh processes and reused output paths; no stale successful facts in diagnostic output. Inject allocation/write/rename failure; previous successful outputs remain identifiable and no partial model is certified. |
| Input integrity | Same-size/timestamp-preserving source edit, symlink retarget, disappearing input and newly present missing probe; model cannot claim freshness from mtime alone. |
| Compatibility | Generated-program differential against pinned 2.0.0 and immediate base for bytes, diagnostics, all existing output formats and option combinations; explicitly inventory every intentional difference. Model grouping cannot silently change existing listings. |

For review, compare exported declaration/name/scope/value evidence against the
assembler's actual resolution/emission path, not just two consumers of the same
export. Prove joins by set equality and uniqueness. Runtime trace completeness,
game identity and agent permission enforcement are outside xasm's acceptance.

## 11. Implementation sequence and promotion gates

1. Review this contract and compatibility ledger independently. Agree the public
   schema, profile and any intentional breaks before production implementation.
2. Implement snapshot-backed source units, parsed coverage and producer scope/
   declaration identities with the standalone round-trip and refusal fixtures.
3. Attach actual resolution/evaluation events and native instruction/data operand
   ranges. Eliminate the spike's consumer-derived ranges and final-table shortcut.
4. Add serialization, diagnostics and output publication with fault controls; run
   the existing suite and bounded generated differential. Measure enabled and
   disabled costs before adding storage machinery.
5. Integrate one migrated consumer against the complete model; show reconstruction,
   bound edits, refresh and structured review without reopening source. Keep
   service permission, budget and document fixes in the consumer repository.
6. Obtain independent xasm review of implementation and evidence. Update release
   notes, CLI/man pages, schema examples and migration documentation together.
   If any public break is accepted, release as 3.0.0 and update consumers in lockstep.

Completion means the contract and its negative controls pass, supported records
come from the normal assembler, unsupported work refuses explicitly, and the
release's compatibility/cost decisions are documented. This branch does not
authorize further model runs, broader platforms, a full assembler-language profile,
remote publication or a release. Promotion branches remain local until the owner
explicitly requests a push.
