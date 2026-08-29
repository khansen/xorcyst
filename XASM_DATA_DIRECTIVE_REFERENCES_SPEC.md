# xasm Structured Data-Directive References Spec

## Status

Implemented and landed in xasm; the coordinated NESrev consumer is also merged.
This document deliberately permits a breaking JSON xref change. xasm and the
NESrev tooling moved to the new contract in sync; no compatibility adapter or
dual-schema transition is required.

A read-only implementation prototype assembled every project at the pinned
NESrev commit and found every tracked `.DW` inventory tuple by lexical owner,
owner-relative index, expression, and mapped target kind. The only additional
`.DW` records were rows already excluded by NESrev's terminal-vector policy;
every emitted `.DW` record had an unambiguous target. Verification derives the
counts from the pinned tree rather than copying them into this contract.

## Goal

Make xasm the authoritative source for symbolic values emitted by `.DB`, `.DW`,
and `.DD` directives. A consumer must be able to identify each symbolic operand,
its emitted position and value, its containing label, and the symbols used by
its expression without reparsing assembly source or joining the structured
listing to ordinary xref rows.

The first consumer is NESrev's `.DW` pointer inventory. It currently reconstructs
directive boundaries, table ownership, operand order, target expressions, and
target kind from source text. That works for common formatting but duplicates
assembler semantics and becomes fragile as source is reformatted or expressions
grow more complex.

This feature is successful when NESrev can replace that reconstruction with one
fresh xasm artifact while preserving the pointer inventory at the pinned NESrev
baseline commit.

## Current Limitation

The JSON listing and xref each expose part of the required information, but
neither is an operand contract:

- the structured listing is byte-row oriented; it may split or combine emitted
  data in ways that do not retain a one-to-one operand identity
- ordinary xref rows are identifier oriented, so one operand may produce several
  rows and a literal operand produces none
- `xref_visit_data` extracts one source operand string before iterating the
  directive's AST children, so current rows can repeat the first expression for
  later operands even though their output offsets and referenced symbols differ
- joining the two artifacts by source line or output offset forces consumers to
  reconstruct facts that xasm already owns

The desired unit is therefore the data-directive operand, not a listing byte row
or an individual identifier reference.

## Pipeline Position and Provenance

JSON xref generation runs after `astproc_third_pass` and, for pure-binary
builds, after `astproc_fourth_pass`. The third pass calls `maybe_merge_data`,
which has two observable effects that this feature must not ignore:

- unless `--debug` is set, adjacent data nodes of the same datatype are merged
  into one `DATA_NODE`
- when a merge run ends, directive expressions are reduced; values that already
  reduce to `INTEGER_NODE` may be truncated in the AST, while label-bearing
  operands can remain symbolic

For pure-binary output, `astproc_fifth_pass` writes the binary before xref
generation. Its `write_data` path evaluates remaining symbolic operands and
truncates a local value without replacing the operand AST. Consequently, xref
cannot assume either that an operand remains structurally intact or that its
encoded value was stored back into the tree.

The merged node is not a source directive statement. The first pass is also an
earlier destructive boundary than its name suggests: `process_data` substitutes
`.EQU` symbols and folds constants. Record provenance must therefore be captured
inside that callback, after macro expansion has materialized the native data
node but before its operand-reduction loop runs. At minimum, the preserved
provenance for each operand contains its directive identity, source location,
`operand_index`, operand-specific expression spelling, referenced symbols, and
the structural target candidate: base symbol, projection, and unevaluated displacement
expression. After pass 1, scoped local/anonymous names are synchronized from the
surviving operand before later reductions. The final xref walk computes
width-scoped lexical-owner indices, adds emitted addresses and offsets, evaluates
the encoded value, validates same-segment label differences, resolves the
numeric displacement, and adds segment identity and target classification.

An implementation may store that provenance on the operand AST node with rules
for copying it to replacement nodes, or assign a durable origin ID propagated
through merging and expression reduction. It must not assume the original node
pointer survives every reduction, and it must not infer source statement
boundaries from the merged `DATA_NODE`. Existing child locations survive
today's merge and remain useful evidence, but grouping only by line number is
not a complete contract for macros or future same-line forms.

`--debug` may suppress merging, but it must not change any
`data_directive_references` record. The parity invocation does not use
`--debug`; a dedicated fixture proves that adding it produces the same new
section.

## Design Principles

1. **One authoritative unit.** Emit one record per reference-bearing directive
   operand, directly while walking that operand's AST.
2. **Assembler facts, not pointer guesses.** Report expression structure,
   symbols, values, locations, and lexical ownership. Do not decide whether a
   value is a gameplay pointer.
3. **No artifact join.** A consumer must not need the listing to interpret a
   directive-reference record.
4. **No extra assembly.** The records ride in the JSON xref already produced by
   a bundled analysis invocation.
5. **Conservative resolution.** Omit optional target fields when the expression
   does not match the restricted single-base grammar. The base may be a label or
   an address-valued `.EQU`; it is identified structurally, not selected from a
   general expression merely because an identifier appears first.
6. **Explicit schema break.** Correctness and a clean contract take precedence
   over preserving the version-1 JSON shape. Version the change and update
   consumers deliberately.

## CLI and Versioning

No new output file or xasm invocation is introduced.

Invocation:

```sh
xasm --pure-binary \
  --xref=Game.xref.json \
  --xref-format=json \
  --xref-include-owner=true \
  --xref-data=true \
  Game.asm
```

For every `--xref-format=json` output:

- change the top-level xref `version` from `"1"` to `"2"`
- make ordinary data-directive `references[].expression` operand-specific rather
  than repeating the whole directive payload

When `--xref-data=true`, also add the top-level
`data_directive_references` array. Without `--xref-data=true`, that section is
omitted as the existing `data_reads`, `data_writes`, and
`indirect_data_flows` sections are today.

Version 2 does not require a version-1 compatibility mode. Existing JSON
consumers are updated in the coordinated NESrev change rather than supported by
a shim. Text and CSV xref formats are outside this feature; their current
behavior may remain unchanged. The new section is JSON-only, like the existing
`--xref-data` extensions.

The version bump applies only to JSON xref. Structured listing JSON and audit
JSON remain at version `"1"`; this feature does not change either artifact.

When requested, `data_directive_references` is present as an empty array if the
build contains no qualifying operands. Its presence is not conditional on
whether matches were found.

## Record Selection

Visit every emitted operand of `.DB`, `.DW`, and `.DD`, including aliases that
map to the existing byte, word, and dword datatypes.

Emit a record when the operand AST contains at least one identifier reference.
Do not emit literal-only operands. Nevertheless, count every operand when
computing indices, including literals and expressions that do not produce a
record. This keeps indices stable and lets a consumer distinguish:

```asm
Table:
    .DW FirstTarget, $0000, SecondTarget
```

as owner item indices 0 and 2 rather than collapsing the symbolic entries to 0
and 1.

`.DSB`, `.INCBIN`, instructions, `.EQU` definitions, and control directives do
not produce records in this section.

## JSON Schema

Required fields:

- `file`: source file owning the operand
- `line`: one-based source line
- `column`: one-based column of the operand when available, otherwise the
  directive's column
- `directive`: canonical uppercase directive (`.DB`, `.DW`, or `.DD`)
- `width_bytes`: emitted width of this operand (1, 2, or 4)
- `operand_index`: zero-based index within this directive statement
- `expression`: the operand-specific source slice with insignificant whitespace
  removed when that slice contains only tokens belonging to the expanded
  operand, preserving numeric spelling, operators, grouping, and `<` / `>`
  projection; use a canonical AST rendering when macro location semantics or
  another transformation makes the source slice broader than the operand
- `referenced_symbols`: identifiers used by the operand, in first-appearance
  order with duplicates removed
- `emitted_value`: the unsigned integer actually encoded after xasm's existing
  datatype truncation in pure-binary mode; in object mode, the unsigned
  width-normalized value at xasm's current address assignment
- `use_cpu_address`: address of this operand's first emitted byte, encoded using
  the existing xref address convention
- `use_output_offset`: output offset of this operand's first emitted byte, or
  `null` when no file offset exists
- `segment_id`: the same stable per-build segment identity used internally by
  xref; this prevents equal CPU addresses in separate segments from being
  conflated

Lexical-owner fields:

- `owner_symbol`: nearest non-local defined label encountered earlier in the
  same segment whose address is not greater than the operand address
- `owner_symbol_addr`: that label's CPU address
- `owner_symbol_output_offset`: its output offset, or `null` when unavailable
- `owner_item_index`: zero-based index among operands of the same directive
  width since `owner_symbol`; literal operands count even though they are not
  emitted as records

The owner fields are emitted only when `--xref-include-owner=true` and an
eligible owner exists; otherwise they are omitted together. This is a lexical
containment statement, not a control-flow or callable-routine claim. For a
conventional pointer table, `owner_symbol` is the table label. It is not
restricted to labels classified as routines.

`owner_item_index` is scoped by owner and directive width, not by merged-node
run. An interposed `.DB` run does not reset the `.DW` index; encountering a new
`owner_symbol` resets the counters for all widths.

Optional unambiguous-target fields:

- `target_symbol`: the base symbol proven by the restricted target-expression
  grammar below
- `target_displacement`: signed constant displacement from `target_symbol`
- `target_projection`: `"none"`, `"low"`, or `"high"`
- `target_kind`: `"code"`, `"data"`, `"equate"`, or `"unknown"`

The restricted target-expression grammar is an optional `<` or `>` projection
around `Base`, `Base + ConstantExpression`, or `Base - ConstantExpression`.
`Base` is one syntactically distinct identifier and may name a label or an
`.EQU`. `ConstantExpression` may contain:

- numeric literals and symbols whose definitions reduce completely to constants
- a parenthesized `LabelA - LabelB` term when both labels are defined in the
  same xasm segment, making their difference link-time constant
- arithmetic composed from those terms and fully resolved constants

Parentheses may group any allowed form. This covers direct labels, label
displacements, `RAM_OamShadowBase+$1E*OAM_SPRITE_STRIDE`, and the baseline
`RAM_Base+(TableLabel-EntryLabel)` idiom without treating the two labels in the
difference as competing bases.

Emit the four target fields only when the operand matches that grammar and all
four fields are known. A sum of two labels, a difference of labels from
different segments, non-constant arithmetic around the base, or an ambiguous
definition omits them. `referenced_symbols`, `expression`, and `emitted_value`
remain authoritative in those cases.

`target_kind` describes the definition at the target symbol:

- `code`: the label's first emitting statement is an instruction
- `data`: the label's first emitting statement is a data/storage/binary directive
- `equate`: the symbol is defined by `.EQU`
- `unknown`: the definition is available but none of the above is provable

Blank lines, comments, and intervening labels at the same address do not by
themselves determine kind. When several labels share a definition address, each
symbol is classified from the same first emitting statement.

## Example

Input:

```asm
.ORG $8000
HandlerTable:
    .DW InitHandler, UpdateHandler-1
    .DW $0000, DrawHandler

SpritePtrLoTable:
    .DB <IdleSprite

InitHandler:
    RTS
UpdateHandler:
    RTS
DrawHandler:
    RTS
IdleSprite:
    .DB $00
```

Relevant version-2 output:

```json
{
  "version": "2",
  "data_directive_references": [
    {
      "file": "Game.asm",
      "line": 3,
      "column": 9,
      "directive": ".DW",
      "width_bytes": 2,
      "operand_index": 0,
      "owner_item_index": 0,
      "expression": "InitHandler",
      "referenced_symbols": ["InitHandler"],
      "emitted_value": 32777,
      "use_cpu_address": "0x8000",
      "use_output_offset": 0,
      "segment_id": 1,
      "owner_symbol": "HandlerTable",
      "owner_symbol_addr": "0x8000",
      "owner_symbol_output_offset": 0,
      "target_symbol": "InitHandler",
      "target_displacement": 0,
      "target_projection": "none",
      "target_kind": "code"
    },
    {
      "file": "Game.asm",
      "line": 3,
      "column": 22,
      "directive": ".DW",
      "width_bytes": 2,
      "operand_index": 1,
      "owner_item_index": 1,
      "expression": "UpdateHandler-1",
      "referenced_symbols": ["UpdateHandler"],
      "emitted_value": 32777,
      "use_cpu_address": "0x8002",
      "use_output_offset": 2,
      "segment_id": 1,
      "owner_symbol": "HandlerTable",
      "owner_symbol_addr": "0x8000",
      "owner_symbol_output_offset": 0,
      "target_symbol": "UpdateHandler",
      "target_displacement": -1,
      "target_projection": "none",
      "target_kind": "code"
    },
    {
      "file": "Game.asm",
      "line": 4,
      "column": 16,
      "directive": ".DW",
      "width_bytes": 2,
      "operand_index": 1,
      "owner_item_index": 3,
      "expression": "DrawHandler",
      "referenced_symbols": ["DrawHandler"],
      "emitted_value": 32779,
      "use_cpu_address": "0x8006",
      "use_output_offset": 6,
      "segment_id": 1,
      "owner_symbol": "HandlerTable",
      "owner_symbol_addr": "0x8000",
      "owner_symbol_output_offset": 0,
      "target_symbol": "DrawHandler",
      "target_displacement": 0,
      "target_projection": "none",
      "target_kind": "code"
    },
    {
      "file": "Game.asm",
      "line": 7,
      "column": 9,
      "directive": ".DB",
      "width_bytes": 1,
      "operand_index": 0,
      "owner_item_index": 0,
      "expression": "<IdleSprite",
      "referenced_symbols": ["IdleSprite"],
      "emitted_value": 12,
      "use_cpu_address": "0x8008",
      "use_output_offset": 8,
      "segment_id": 1,
      "owner_symbol": "SpritePtrLoTable",
      "owner_symbol_addr": "0x8008",
      "owner_symbol_output_offset": 8,
      "target_symbol": "IdleSprite",
      "target_displacement": 0,
      "target_projection": "low",
      "target_kind": "data"
    }
  ]
}
```

The literal `$0000` produces no record, but it still advances
`owner_item_index`, which is why `DrawHandler` has index 3.

## Ordering and Duplicate Rules

- Sort records by segment encounter order, then `use_output_offset` when
  present, then CPU address, source location, and operand index.
- Preserve two identical operands at different emitted positions as distinct
  records.
- A repeated identifier inside one operand appears once in
  `referenced_symbols`; the operand itself still produces only one record.
- Macro expansion must not merge operands or borrow an expression from a
  sibling expansion. Source location may point to the macro invocation or
  definition according to existing xasm location semantics, but emitted
  position, operand indices, expression, and referenced symbols must describe
  the expanded operand that produced the bytes.

## Implementation Direction

The implementation should add provenance around the existing pass pipeline and
extend the xref data walk rather than parse source text after assembly:

1. Register a first-pass data-analysis hook. When `process_data` receives an
   expanded native data node, capture each original operand before its
   substitution/folding loop, assign a durable origin ID, and preserve the
   structural target candidate and displacement expression. After pass 1,
   reconcile finalized local/anonymous names from each surviving operand.
2. Add a `data_directive_reference` collection to `xref_build_context`, and
   make the post-pass `xref_visit_data` lookup the preserved provenance for
   each surviving RHS operand.
3. Collect ordered identifiers from the original operand AST. Split the
   directive source at top-level commas, retain the selected operand with
   insignificant whitespace removed only when every identifier-like token
   belongs to that expanded operand, and otherwise use a canonical AST
   rendering. Do not reuse the current whole-directive
   `extract_operand_from_line` result for every child. This distinction
   preserves `$1E` and deliberate grouping in normal source while preventing a
   macro invocation name from becoming part of its expanded operand expression.
4. Compute `emitted_value` during xref by evaluating the final operand without
   replacing it in the AST, using the completed xref symbol/address map when an
   object-mode symbol-table entry has no final-address flag, then applying the
   same datatype-width truncation as `write_data`. Prefer a shared pure helper
   so code emission and xref cannot drift. Do not issue a second truncation
   warning, and do not attempt to expose the pre-truncation value.
5. Track the nearest eligible owner symbol in the current segment. Reuse a
   prebuilt symbol/owner index during output rather than performing an O(N)
   search for every record.
6. After final addresses exist, validate and resolve the preserved target
   candidate through the restricted expression analysis. Failure to prove one
   base or a same-segment displacement is a normal omitted-field result.
7. Emit the new array from the JSON writer and bump only the xref schema
   version.
8. Correct ordinary data-directive reference expressions using the same
   preserved operand rendering so the two xref views cannot disagree.

The change is analysis-only. It must not alter AST evaluation, emitted bytes,
relocation behavior, warnings, or listing output.

## Test Matrix

Extend `tests/regression.sh` with exact or structurally asserted JSON fixtures
covering:

- consecutive single-operand `.DW` lines: each record carries its own expression
- one `.DW` with several operands: distinct operand and output positions
- a literal between symbolic operands: no literal record, stable index gap
- consecutive `.DW` statements with and without `--debug`: identical new
  records and per-statement `operand_index` values
- `.DW`, interposed `.DB`, then `.DW` under one owner: the word-relative owner
  index continues across the byte run
- `.DB <Target` and `.DB >Target`: low/high projection and full target identity
- a symbol-plus-constant displacement, including a symbolic `.EQU` constant
- `EquateBase+(LabelA-LabelB)` with both labels in one segment: target fields
  present and displacement resolved
- a label sum and a cross-segment label difference: symbols retained, target
  fields omitted
- `.DB FarLabel` where the label exceeds one byte: existing truncation warning
  emitted exactly once, `emitted_value` is the encoded byte, and target identity
  survives
- code, data, equate, and unknown target-kind classification
- two labels at one address and an operand under each lexical owner
- a segment restart with a repeated CPU address: owners do not cross segments
- local/anonymous references under the existing include flags
- macro-expanded operands: no expression or index borrowing between siblings
- pure-binary and non-pure-binary modes: output offset present versus `null`
- empty-result output: the version-2 array is present and empty

Every fixture must also assert output-byte identity against a control assembly.
At least one negative mutation should make the consecutive-`.DW` expression
test fail by restoring the current whole-line expression reuse.

Build before running the suite:

```sh
./configure && make
sh tests/regression.sh
```

## Downstream Migration Contract

The xasm feature and the NESrev consumer may be separate commits or reviewable
PRs, but they form one lockstep tooling change. Acceptance is cross-repository:

1. xasm implements the version-2 schema and fixture coverage.
2. NESrev updates its required xasm contract and produces the JSON xref once
   from the current post-edit source during inventory/verification and shares
   that fresh artifact with consumers.
3. NESrev migrates only the `.DW` pointer inventory first.
4. The frozen baseline is NESrev commit
   `9a6c1f649baca9d076682ce0a1af81fb402c3520`. It contains both the version-2
   consumer and the reviewed Kung Fu terminal-vector correction, so the pinned
   tree passes its own inventory-regeneration gate. Derive the row count from the
   tracked `projects/*/docs/reverse_engineering/inventory/pointer_targets.csv`
   files at that commit; do not copy a count into the implementation.
5. With the invocation shown in this spec and no `--debug`, the structured
   implementation must reproduce every baseline row exactly, including owner,
   entry index, expression, and target classification. If an old scanner result
   needs correction, make and review that correction in NESrev first, then pin
   a new baseline commit; do not hide a difference inside the migration.
6. The xasm and NESrev changes land or are deployed together; no supported state
   pairs version-2 xasm with a version-1 NESrev JSON consumer.
7. Only after that parity gate may NESrev consider the embedded and split `.DB`
   pointer inventories.

The pinned corpus contains no cross-segment label-difference operand. Corpus
parity therefore cannot prove the refusal half of the same-segment rule; the
dedicated cross-segment fixture is its required gate.

NESrev-specific policies such as excluding terminal CPU vectors remain consumer
policy. xasm emits their directive operands normally and stays architecture
neutral. For baseline compatibility, NESrev maps xasm `code` to `code_pointer`,
`data` or `equate` to `data_pointer`, and `unknown` or omitted target fields to
`unknown_pointer`; xasm itself does not emit those NESrev classifications.

## Non-goals

- Classifying every symbolic data operand as a pointer
- Replacing source-text checks whose subject is spelling or formatting
- Detecting branch literals such as `$+N`
- Migrating readability/style checks to semantic analysis
- Emitting every literal data byte as a JSON record
- Reporting a pre-truncation value that xasm does not retain as an output fact
- Adding a persistent structured listing cache
- Migrating all NESrev scanners in one campaign
- Making control-flow reachability claims from lexical ownership

## Acceptance Criteria

- JSON xref version 2, when invoked with `--xref-data=true`, exposes one
  operand-centric `data_directive_references` record for every qualifying
  `.DB`, `.DW`, and `.DD` operand.
- Each record has the correct operand-specific expression, emitted value,
  address/offset, directive index, owner-relative index, referenced symbols,
  and lexical owner.
- Optional target fields are correct when present and absent rather than guessed
  for ambiguous expressions.
- Consecutive directives and multi-operand directives cannot repeat a sibling's
  expression while reporting a different symbol or output offset.
- The feature adds no xasm invocation and requires no listing/xref join.
- xasm regression tests, byte-identity checks, and warning-set parity checks
  pass. Every label-preserving analysis mode must report the same unused-label
  warning set as a plain assembly while retaining the nodes its artifact needs.
- A coordinated NESrev prototype demonstrates exact `.DW` inventory parity at
  the pinned NESrev commit before the source scanner is removed.
