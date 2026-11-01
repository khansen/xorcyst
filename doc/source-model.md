# Lossless source model, version 1

`xasm --pure-binary input.asm -o output.bin --source-model=model.json`
exports the assembler's lossless source and typed facts in a separate JSON
document. This is an opt-in interface; it does not replace listing, xref,
instruction records or XOROBJ2. Existing assembly acceptance and existing
analysis schemas are unchanged. This change does not require a major-version
bump. No release is authorized by the implementation branch.

The [approved specification](../XASM_SOURCE_MODEL_SPEC.md) defines semantics;
[the JSON Schema](source-model.schema.json) defines the wire shape. The schema
uses Draft 2020-12. Reject unknown versions, profiles, enum values and fields
instead of silently interpreting a different contract. Examples and neutral
fixtures live in `tests/fixtures/source-model/` and `tests/test_source_model.py`.
The saved example normalizes invocation/dependency paths to `/example` and
`/usr/local/bin/xasm`; its source bytes, typed tables and content hashes are
actual producer output, not a receipt for a consumer run.

## Invocation and status

`--source-model-profile=asm-edit-v1` is the default and currently the only
profile. `--source-model-diagnostics=repair.json` optionally writes a separate
diagnostic model after a source/certification failure. Both paths participate in
input/output collision detection and output staging. A failed invocation never
makes an earlier model current. Match the invocation result, model identity,
input hashes and binary hash; do not infer success from a file's existence.

The profile requires pure-binary mode and refuses command-line definitions.
Labels (global, local and anonymous), single immutable EQUs, 6502 instructions
including `.W`, explicit byte/word data operands, `.ORG`, `.END`, comments and
formatting are supported. Directive aliases sharing the same native token,
such as undotted `DB` or `.WORD`, are supported. `.DEFINE`, variable declarations,
storage shorthand, macros, includes, conditionals and anonymous references
inside EQUs are not. Unsupported constructs refuse with
`SOURCE_MODEL_UNSUPPORTED_CONSTRUCT`; duplicate named declarations refuse with
`SOURCE_MODEL_DUPLICATE_DECLARATION`, including identical EQUs. Ordinary assembly
without the model option retains its existing behavior.

Successful static models can accompany exits 0, 4 (audit findings) and 5
(comparison mismatch). Check `analysis_outcomes` explicitly. In particular,
exit 4 can skip a requested comparison and record `not_run`. A parity-preserving
edit service must require comparison **`equal`**, not merely a certified model
or matching output length. Require a whole-binary comparison: `equal` with
`--compare-length` or offset/window options certifies only the requested window.
Certification makes no reference-ROM parity claim.
Exits 1, 2, 3 and fatal internal failures authorize no certified output. Diagnostic
models have null evaluations, segments, locations, instructions, data, emissions
and binary; they may be used for source reconstruction and repair only.

## Records and reconstruction

File bytes are stored once as base64, with byte length and SHA-256. `units`
partition each file completely and in byte order. Reconstruct by concatenating
their slices of those embedded bytes. Never read the original path to fill a
gap. Comments and byte strings need not be UTF-8; byte offsets are authoritative.
LF alone is a newline. A CR accepted inside a comment remains a comment byte.
No grammar changes were made to accept BOMs, bare CRLF or missing final LF.

`syntax` records preserve the parsed tree before folding. Declarations, scoped
references and complete expression trees join to it by ID. Resolution uses the
assembler's resolver, including a side-effect-suppressed pass over unused EQU
expressions. An unused unresolved or cyclic EQU refuses certification even when
ordinary assembly accepts it. Local names retain declaration scope; anonymous
spellings form separate forward/backward chains (`+` and `++` are distinct).

`evaluations` record native values and PC contexts for every expression, with
reference-value evidence. In an EQU expression containing no syntactic `$`,
`pc_context.cpu_address` is null: the value does not depend on that context's PC,
and a native first-pass estimate may be stale after address-mode narrowing.
This includes aliases of other EQUs; the referenced EQU retains its own context.
All other evaluation contexts carry the exact PC, including every subexpression
of an EQU containing `$`. Native xasm refuses `$` at an unstable EQU declaration
point. `segment_id` remains present even when the PC is null.
`locations` identify a syntax record by segment,
CPU address, output offset and length. Always qualify a CPU address by segment.
Segment `g1` starts implicitly at address/output offset zero. Every `.ORG` starts
a new segment, even when origins repeat. A label before an `.ORG` remains at
the preceding PC. Segments do not imply physical cartridge banks or runtime
mapper selection, and `.ORG` gaps do not add emitted padding.

Instruction/data operands have their own expression/evaluation IDs and native
byte ranges. Empty strings have empty ranges. `emissions` partition the binary
into positive ranges for instruction opcodes, instruction operands and data
operands. Existing listing grouping is independent of these source identities.
`forced_absolute` records that `.W` was written; it is true even for `DEX.W` or
`LDA.W #1`, whose encoding is not absolute. Access classes use the same native
classification as instruction records, including pointer reads and RMW, but
addresses and control-flow targets describe the encoded operand (branch targets
wrap to 16 bits). Expression evaluations retain their native untruncated values.

IDs are deterministic preorder/collection ordinals within one model, not stable
cross-edit symbol identifiers. Prefixes distinguish files (`f`), units (`u`),
syntax (`s`), scopes (`q`), declarations (`d`), expressions (`e`), references
(`r`), evaluations (`v`), locations (`l`), segments (`g`), instructions (`i`),
data statements (`b`), operands (`o`) and emissions (`m`). Consumers must retain
edit provenance or align typed records when comparing different models.

## Canonical identity

Each hash field is UTF-8, prefixed by its **decimal byte length**, a colon, then
its bytes, with no other delimiter. SHA-256 hashes the concatenated fields.
Integers in this length encoding have no leading zeroes. The fixed vectors in
`tests/fixtures/source-model/identity.json` test this encoding independently.

`input_id` fields, in order:

1. `xasm-source-model-input-v1`;
2. `case-sensitive` or `case-insensitive`;
3. `native-parens` or `swapped-parens`;
4. each file's logical path and SHA-256, in acquisition order.

The certified v1 profile has one source file, whose logical path is its basename.
A diagnostic may capture additional consumed files: each gets its file-ID prefix
(for example `f2/input.asm`), so equal basenames remain distinct. This does not
enable includes in certified models. The invocation/dependency evidence
also records actual paths, commands and consumed hashes; those absolute paths
are excluded from identity. A relocation with identical logical filename and
bytes therefore preserves input and record identities.

`model_id` fields are `xasm-source-model:1:asm-edit-v1`, `input_id`, and the
running producer executable's SHA-256. `schema_id` and `profile_id` are explicit
as well. Comparison/reference paths, audit selection, diagnostics, output paths,
verbose/debug settings and optional old products do not change these semantic
identities. Their outcomes and invocation evidence remain separately visible.

## Migration and bounded consumer example

Consumers must first validate schema/status, reconstruct and verify the source
partition, compare parsed inventories with exported tables, and check binding,
scope, segment and byte-range consistency. Do not reconstruct missing facts by
parsing the assembly text. The independent test-only reader in
`tests/source_model_support.py` is an oracle for this producer contract, not an
installed service fallback or a second production assembler.

`tests/test_source_model_consumer.py` demonstrates model-only reconstruction,
a declaration-bound rename, refreshed assembly, parity checking and a structured
before/after review with the original source unavailable. The fixture uses no
game-specific names or paths. It is a migration example; NESrev's production
service extraction, permissions, budget accounting and broader edit operations
remain changes in the consumer repository.

## Resource and publication limits

Capture allocates only when requested. Tables are bounded by 32-bit IDs and
checked host allocation sizes; model syntax depth is limited to 512. Allocation,
overflow or depth exhaustion refuses with `SOURCE_MODEL_RESOURCE_LIMIT` and does
not publish a truncated diagnostic capture. The normal parser/backend may reject
inputs earlier under their existing limits. Unresolved facts use
`SOURCE_MODEL_UNBOUND_REFERENCE` / `SOURCE_MODEL_VALUE_UNAVAILABLE`; coverage
failures use `SOURCE_MODEL_INCOMPLETE`. Collection exhaustion exits 3, with or
without a diagnostics path. Failed source acquisition reports
`SOURCE_MODEL_IO_ERROR` and publishes no empty capture. Output I/O failures use
the same code; changed captured inputs use `SOURCE_MODEL_INPUT_CHANGED`.
Internal layout/capture invariant failures use `SOURCE_MODEL_INTERNAL_ERROR`,
separately from an invalid or unsupported source expression.

Outputs are staged through the existing output batch. Detected I/O, input-change
and publication failures preserve the previous successful set. This is not
multi-file crash durability. Models from interrupted invocations are not usable
until a fresh invocation establishes completion and identity.

The format deliberately starts as uncompressed JSON. Measure retained model
bytes, peak scratch storage and consumer memory as well as producer latency.
Private corpus captures are optional evidence and are not distributed. The corpus
runner retains only hashes, counts and timings, and removes each temporary model
before proceeding. Downstream full-service rebuild/amend cost must be measured
in that service; assembler timings do not establish a service latency budget.
