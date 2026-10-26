# Migrating to XORcyst 2.0

This is an unreleased cutover candidate. Release publication follows local review,
owner approval and green release checks.

Build and install the usual `xasm` and `xlnk`. The alternate `xasm-v2`/`xlnk-v2`
targets and compile-time frontend selector are gone. Reassemble every object from
source: the linker accepts XOROBJ2 only. There is no reader or converter for the
old format. Existing linker-script commands remain; numbers must consume the
entire token (`$8000`, not `$8000x` or `8000h`).

## Source behavior

The approved compatibility ledger in `XORCYST_V2_CONTRACTS.md` is authoritative.
The main changes are:

- Forward string data resolves before layout, with character maps captured at
  each use. Every string operand in `.char` is mapped. Integer/string `+` keeps
  concatenation; string multiplication and division are type errors.
- Direct binary assembly repeats the same instruction-narrowing walks as the
  linker. Cascades may shorten additional instructions.
- Source-time `$` uses a known final PC. Unresolved widths reject captures and
  expansion-time uses; `$` is prohibited in type dimensions. Data/instruction
  operands still receive their own final PC.
- `.label name = address` is rejected. Use an equate for a fixed address.
- `sizeof(label)` stays a source-time scan. Local forward numeric/address
  operands retain fixed widths. Imported kinds, deferred strings, changed kinds
  or string lengths, and strings built from their own size reject at the use.
- Imported/exported aliases and string initializers use the provider's value
  kind. `.extrn` annotations remain local typing hints.

Binary/object differences specifically retained by the ledger remain. `.org`
and source-time absolute PCs are binary-only; bank expressions and imports
require linking. CPU addresses are not file offsets.

`xlnk --verbose` reports linked/copied inputs and published output paths. It
no longer prints the old RAM symbol-map dump.

Linker errors remain located, but some messages are less specific: backward
padding and bank overflows report a layout limit, invalid RAM ranges report
`invalid RAM block`, and missing bank sizes report `invalid bank size or origin`.
Scripts must use a distinct path for each `output{}` and cannot `copy{}` an
output of the same invocation. Write an intermediate file in a separate link
invocation when a later step needs to consume it.

Missing local names and implicit external names reached through captured aliases
are diagnosed at the alias definition, rather than at its later use. Multiple
missing-local diagnostics can appear in reverse source order.

## Analysis consumers

Remove `--listing-version` from invocations. JSON and NDJSON listings always use
schema `"2"`; instruction records remain schema `"3"`. Validate schema identifiers
before reading a product. Structured products and text listings require
`--pure-binary`; object-mode analysis is refused with previous files preserved.

Non-emitting listing rows have **null output offsets** and empty byte arrays.
Join only emitting spans to the binary, using inclusive `output_offset_end`.
Storage rows describe zero fill with `fill_byte` and `byte_count`. Instruction
addressing modes describe the emitted opcode, including zero-page shortening
and accumulator modes. See `README` for the complete row contract.

A diagnostic listing has no final bytes or offsets. Its CPU extents are
provisional. Some errors prevent a diagnostic listing; always check exit status.
Consumers should verify the manifest and artifact hashes as one set. Detected
assembler collection and I/O failures preserve the previous product set;
compare/audit findings still publish their requested results. If assembler
rollback itself fails, it reports and retains its backup for recovery. The
linker stages all outputs before publishing them, but publication is per file:
a later rename failure can leave an earlier output published.

## Remaining limits and costs

Inherited parser and some shared collector allocation failures are not all
reported reliably. Leading unnamed RAM data containing `$`, duplicate size
errors, and exponential expansion of shared aliases in rich analysis remain
tracked in `TODO`. These are known limitations, not new compatibility promises.

Objects are larger because they carry fixed-width tables, expression graphs,
fixups and diagnostic locations. The largest input buffer remains live briefly
while linking translates it. The accepted small-link memory and millisecond
costs, plus per-tool measurements, are recorded in the contract and performance
protocol. Large-project analysis remains subject to the below-1.30× memory gate.
