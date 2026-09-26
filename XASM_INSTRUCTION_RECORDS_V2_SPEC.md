# Instruction records version 2: memory access and additive operand terms

Status: implemented. Extends
[instruction records version 1](XASM_INSTRUCTION_RECORDS_SPEC.md). Everything
not stated here keeps its version 1 meaning.

## Motivation

Version 1 records give each emitted instruction's final mode, operand value,
pre-fold expression tree, lexical owner and emitted bytes. Two facts that
downstream analyses need are still left to each consumer:

1. **What memory the instruction touches, and how.** Consumers re-derive
   read/write behaviour from the mnemonic, and every copy of that table can go
   wrong on its own; read-modify-write instructions are the easiest to
   miscount. xasm's own classifier
   (`classify_instruction_access` in `listing.c`) has the same class of gaps,
   and it feeds the xref, the xref summary, index patterns and data consumers:
   - read-modify-write opcodes (`ASL`, `LSR`, `ROL`, `ROR`, `INC`, `DEC`) fall
     through to `other`, so every one of those outputs omits them;
   - the prefix test "three letters starting with `B`" classifies `BIT`, a
     memory read, as a branch, so a label read only by `BIT` is summarised as a
     jump target (`BRK` also matches, but takes no operand, so it never
     produces a reference);
   - the read test for compares checks the prefix `CP`, which `CPX` and `CPY`
     match but `CMP` does not, so every `CMP` operand is `other` and produces
     no data edge, index-pattern site or data-consumer site;
   - `JMP [addr]` is a `jump`, so its pointer is summarised as a jump target,
     and `STA [zp],Y` counts as a write to a pointer the instruction only reads.
2. **Which named value an operand is built from.** `structural_base` is
   deliberately limited to `symbol ± integer`. An operand written as
   `RAM_Records + FIELD_STATE` or `Table + (SLOT_SIZE * 2) + FIELD` has a null
   base, so consumers pick a name out of `referenced_symbols` and look it up in
   the final xref symbol table. Version 1 explicitly warns against that binding,
   because xasm symbols can be reassigned between instructions.

Version 2 makes the assembler the single owner of both facts.

## Versioning

- The records section and the standalone `--instruction-records-output`
  document both carry `"version":"2"`. Version 1 is no longer produced and
  there is no compatibility switch; the options, invocation rules, ordering,
  coverage and failure behaviour of version 1 are unchanged.
- Every record gains two required fields, `memory_access` and
  `additive_terms`. All version 1 fields keep their names and meaning,
  including `structural_base`, which stays the restricted written-syntax view.
- The legacy xref, xref summary, index-pattern and data-consumer documents keep
  their versions, but their access classification changes as described under
  [Shared classifier](#shared-classifier). These are intentional output
  changes, not compatibility breaks to guard against.

## `memory_access`

Null, or an object describing the data memory the instruction accesses:

```json
"memory_access": {
  "data": {"kind": "read_modify_write", "address": 16, "index_register": null, "via_pointer": false},
  "pointer": null
}
```

### `data`

Null when the instruction accesses no data operand, otherwise:

| Field | Meaning |
|---|---|
| `kind` | `read`, `write` or `read_modify_write`, from the table below. |
| `address` | For direct and indexed modes, `operand_value`: the base address before any runtime index. Null when the data address comes from a pointer. |
| `index_register` | `"X"` or `"Y"` when a runtime index is added to the data address (`zeropage_x/y`, `absolute_x/y`, `postindexed_indirect`), otherwise null. |
| `via_pointer` | True for `preindexed_indirect` and `postindexed_indirect`. |

Zero-page indexed addresses wrap within page zero at run time; absolute indexed
addresses may cross pages. `address` never includes the index and never guesses
it.

| Kind | Mnemonics (memory modes only) |
|---|---|
| `read` | `LDA`, `LDX`, `LDY`, `ADC`, `SBC`, `AND`, `ORA`, `EOR`, `CMP`, `CPX`, `CPY`, `BIT` |
| `write` | `STA`, `STX`, `STY` |
| `read_modify_write` | `ASL`, `LSR`, `ROL`, `ROR`, `INC`, `DEC` |

No official read-modify-write opcode has a pointer mode, so a `data` object with
`via_pointer` true is always `read` or `write`.

`memory_access` is null for implied, accumulator, immediate and relative
instructions, and for `JSR` and absolute `JMP`, whose operand is a control
transfer target that the xref `references` section already records. Stack
pushes and pulls, `BRK`/`RTI`/`RTS` and interrupt vectors are not modelled.

### `pointer`

Non-null only for the three pointer modes, describing the two bytes read to
form an address:

| Field | Meaning |
|---|---|
| `address` | `operand_value`: the location of the pointer's low byte, before any runtime index. |
| `high_byte_address` | Where the high byte is read when the index is zero. |
| `index_register` | `"X"` for `preindexed_indirect`, whose index applies to the pointer location; otherwise null. |

The high byte follows the 6502 wrap rules, stated here so no consumer
re-derives them:

- `preindexed_indirect` and `postindexed_indirect`: `(address + 1) & $FF`,
  staying in page zero.
- `JMP [addr]`: `(address & $FF00) | ((address + 1) & $FF)`; a pointer at
  `$xxFF` takes its high byte from `$xx00`. `data` is null because the fetched
  address is a jump target.

Pointer bytes are always reads. For `preindexed_indirect` the bytes read at
run time are `(address + X) & $FF` and `(address + X + 1) & $FF`.

## Shared classifier

One table-driven classifier replaces `classify_instruction_access` and feeds
every output that calls it today. Its input is the opcode byte, from which the
opcode table gives the mnemonic and final addressing mode (never string
prefixes), plus the operand for the one rule an opcode cannot decide: an
immediate operand whose root is a `low_byte` or `high_byte` operator loads a
pointer byte.

That operand rule keeps today's input, the operand after constant substitution.
It can therefore differ from `additive_terms.projection`, which describes the
written operand: with `PTR_LO .EQU <Table`, `LDA #PTR_LO` is `pointer_lo` in
the xref, whose reference names `Table`, while its record has projection `none`
and a single term, `PTR_LO`.

### Reference access

`references[].access` for an instruction operand:

| Instruction | `access` | Change from today |
|---|---|---|
| `JSR` | `call` | none |
| `JMP addr` | `jump` | none |
| `JMP [addr]` | `read` | was `jump`; the operand names the pointer, which is read |
| Relative branch | `branch` | none |
| Immediate with a `low_byte` / `high_byte` root after substitution | `pointer_lo` / `pointer_hi` | none |
| Any other immediate | `immediate` | new value; was `read` (`other` for `CMP`), though no memory is read and the symbol's value is used as a number |
| Direct or indexed memory mode | `data.kind` | `read_modify_write` is new (was `other`); `BIT` was `branch`; `CMP` was `other` |
| `[zp,X]`, `[zp],Y` | `read` | was the kind of the access through the pointer, so `STA [zp],Y` was a `write` of the pointer |

`address_compute` keeps its single existing meaning: a symbol used in a data
directive's expression, such as `.DW Handler`. Those references have null
`opcode` and `addressing_mode`. Immediates get their own value so a consumer
that treats `address_compute` as code-pointer evidence never has to separate
the two. Accumulator and implied instructions have no operand and so no
references.

### Effect on each output

- **xref data edges.** `data_reads` and `data_writes` cover direct and indexed
  modes only, as today. A `read_modify_write` access to a data label emits both
  a `data_reads` and a `data_writes` edge with the same site, and a `BIT` or
  `CMP` access now emits a `data_reads` edge. Pointer modes still emit no data
  edge.
- **Pointer-pair tracking and `indirect_data_flows`.** Only a `write` sets a
  pointer byte; `read_modify_write` never updates the pointer-pair state.
  `INC ptr+1` advances a pointer that is already set up, as in the page step of
  a copy loop, so a later `[ptr],Y` access keeps its flow from the original
  stores. Each flow's `access_kind` is the `data.kind` of the access through
  the pointer: `read` or `write`, as today, and no longer tied to the reference
  access for the pointer.
- **xref summary.** Counts follow the reference table. A `read_modify_write`
  reference adds one to both `read_count` and `write_count` and marks the label
  a data label; `total_ref_count` counts it once. `BIT` and `JMP [addr]`
  references now count as reads, so a label referenced only by them moves from
  `top_jump_targets` to `top_data_labels`. `CMP` references count as reads too.
  A pointer used by `STA [zp],Y` gains a read and loses a write. `immediate`
  references, like `address_compute`, count only toward `total_ref_count`, so a
  label used only as an immediate value no longer ranks as a data label.
- **Index patterns.** A `read_modify_write` access, such as `INC Table,X`,
  becomes a supported site with `access_kind` `read_modify_write`; today it
  produces no record. Pattern selection treats it like a write: the paired-byte
  and split low/high patterns still anchor and match only on `read` sites, and
  the scaled-stride and base rules apply unchanged. The record carries the
  `write_access` evidence flag, because the instruction writes the table. Index
  bounds are resolved for it as for any other site. A `CMP` access is now a
  `read` site like any other load.
- **Data consumers.** A `read_modify_write` site is listed in both
  `read_sites` and `write_sites`, and counted in both `read_site_count` and
  `write_site_count`, matching the data edges. A `BIT` or `CMP` site is listed
  in `read_sites`.

## `additive_terms`

Null for operandless records. Otherwise an object that decomposes the operand
expression into signed terms:

```json
"additive_terms": {
  "projection": "none",
  "terms": [
    {"sign": 1, "kind": "symbol", "name": "RAM_Records", "value": 768,
     "binding": {"kind": "constant",
                 "definition": {"file": "main.asm", "line": 12, "column": 1, "end_line": 12, "end_column": 26}},
     "source": {"span": {"file": "main.asm", "line": 40, "column": 9, "end_line": 40, "end_column": 20},
                "text": "RAM_Records"}},
    {"sign": 1, "kind": "symbol", "name": "FIELD_STATE", "value": 1,
     "binding": {"kind": "constant",
                 "definition": {"file": "main.asm", "line": 14, "column": 1, "end_line": 14, "end_column": 16}},
     "source": {"span": {"file": "main.asm", "line": 40, "column": 23, "end_line": 40, "end_column": 34},
                "text": "FIELD_STATE"}}
  ]
}
```

### Decomposition

1. If the expression root is a `low_byte` or `high_byte` operator, record
   `projection` as `low` or `high` and decompose its operand; otherwise
   `projection` is `none`. Only one outer projection is stripped. `bank` is not
   a projection. It takes only a symbol, and pure-binary assembly, which records
   require, cannot resolve a symbol's bank, so it never reaches a record.
2. Flatten binary `+` and `-` and unary `negate` into terms, propagating signs
   through nested groups: `A - (B + C)` yields `+A`, `-B`, `-C`, and
   `-(A - 2)` yields `-A`, `+2`.
3. Every other node becomes one term. A single symbol or integer is a
   one-term decomposition.

Terms are in source order. Term `kind` mirrors the expression node kind for
leaves (`symbol`, `local_symbol`, `forward_label`, `backward_label`, `integer`,
`string`, `current_pc`); any other subtree, such as `SLOT_SIZE * 2`, a member
access or a scoped name, is one term of kind `expression`.

### Term fields

| Field | Meaning |
|---|---|
| `sign` | `1` or `-1`. |
| `kind` | As above. |
| `name` | The written name for symbol-like kinds, else absent. Not a stable cross-build ID. |
| `value` | The term's value, before projection and truncation; see [Capture points](#capture-points). |
| `referenced_symbols` | For kind `expression`: ordered, unique symbol spellings inside the term. Absent otherwise. |
| `binding` | Present for symbol-like kinds, else absent. For `symbol` and `local_symbol`: `kind` (`label`, `constant`, `procedure`, `variable` or `enum_member`) and `definition`, the span of the definition this use assembled (for a procedure, its `.PROC` statement), or null when xasm has no source location (for example a command-line define). An `enum_member` binding also has `enum`, the enumeration's name. Null for anonymous labels. |
| `source` | The term's parsed span and text. Like version 1 expression spans, it excludes grouping parentheses. |

Those five binding kinds are the only ones a bare symbol term can have. xasm
accepts a bare enum member (`LDA #GREEN`) and rewrites it to `Color::GREEN`
before translation. It rejects a bare struct, union, enum or record type name
("operand does not evaluate to literal") and a bare record field, and a macro
name cannot appear in an expression. Written scoped, member, `sizeof` and
`mask` forms, such as `Color::GREEN`, are `expression` terms with no binding.

`binding` is what removes the version 1 hazard: a consumer that needs "this
operand is built from a RAM constant" reads the term's name, binding kind and
value directly, without consulting the final symbol table.

### Capture points

xasm evaluates an operand in stages, so no single evaluation yields both the
term values and `operand_value`:

1. Version 1 copies the operand before folding.
2. `process_instruction` substitutes constants (`substitute_defines`) and folds
   what it can.
3. `translate_instruction` completes the reduction. Labels stay symbolic:
   their addresses are assigned in a later pass.
4. The xref builder evaluates the completed operand, with label addresses and
   at the instruction's PC, for `operand_value`.

Structure and spans come from the pre-fold copy, so decomposition sees the
written tree. Bindings and values cannot be read off the live operand:

- Folding merges terms. With `VALUE = 1`, `process_instruction` has already
  turned `VALUE + 1 + Fwd` into `2 + Fwd` before `translate_instruction` runs.
- Live nodes cannot be matched to terms by source span. `substitute_ident`
  gives the substituted copy the use's location, so a macro argument that
  appears twice in one operand yields two nodes with the same span.

The producer therefore uses a shadow reduction:

1. **Shadow terms.** In the instruction-analysis hook, where version 1 copies
   the operand, the producer also clones each term's subtree. The pre-fold
   copy keeps the written tree; the shadow terms are the ones reduced.
2. **Process stage.** Right after `process_instruction` reduces the operand,
   each shadow term is reduced with the same call (`reduce_expression`,
   `FOLD_PC_NO`) under the same symbol table and scope.
3. **Name rewrites between the stages.** Shadow terms are outside the AST, so
   the pass 1 and pass 2 walks never visit them. The producer applies the
   rewrites those walks make to the live operand:
   - Local label references (`globalize_local`) and backward anonymous
     references (`process_backward_branch`) are renamed in pass 1, by the walk
     of the instruction's children that immediately follows the hook, from
     state that is the same in both places. The shadow applies the same
     rename in the hook.
   - A forward anonymous reference (`process_forward_branch`) is patched in
     place when its declaration is reached later in pass 1, to the name the
     declaration gives its label: its spelling followed by
     `#<branch scope id>#<counter>`. Both numbers are fixed when the reference
     is registered: the id of the current branch scope and that level's
     declaration counter. In a successful build the reference and its
     declaration share a branch scope, and therefore a spelling, including any
     macro-expansion suffix. The shadow computes the name in the hook from the
     reference's spelling and that state, and keeps no pointer to the live
     node.
   - A bare enum member is replaced by an `Enum::Member` scope node in pass 2
     (`validate_ref`). The shadow applies the same rewrite at the start of the
     translate stage.

   Pass 2's other replacements either follow an error, when no records are
   published, or replace `defined[X]` with its result. The shadow needs no
   rewrite for that one: its translate-stage reduction evaluates `defined[X]`
   the same way (`reduce_index`) against the same symbol table. Pass 2 changes
   the table only by entering an undeclared name as an external, which makes
   pure-binary assembly fail.
4. **Translate stage.** In `translate_instruction`, after those rewrites, each
   shadow term is reduced with `reduce_expression_complete` and the same PC
   folding as the real operand.
5. **Values.** Where the xref builder evaluates `operand_value`, it evaluates
   each shadow term with the same evaluator, at the same PC. The result is the
   term's `value`.
6. **Bindings.** A hook in `substitute_ident` records the binding when it
   replaces a shadow term's own identifier, at whichever stage does so: step 2
   for a constant already assigned, step 4 for a forward reference. That is
   the definition xasm assembles, recorded per use and never looked up later.
   Substitutions inside the substituted definition belong to that definition
   and do not rebind the term: with `A = B + 1` and `B` assigned later, the
   term `A` still binds to `A`, and `ALIAS .EQU ORIGINAL` binds to `ALIAS`. Labels, procedures and variables are never
   substituted; their binding comes from the symbol that the resolved name
   refers to at step 4. An enum member's binding is recorded by its step 3
   rewrite.
7. **No side effects.** Shadow work prints no diagnostics and leaves the error
   and warning counts, symbol use counts (`ref_count`), the symbol table and
   branch registration unchanged. Without that, `sizeof`, `mask` and scope
   errors would print twice and break warning parity. A producer flag checked
   by `err`, `warn` and `substitute_ident` is enough.
8. **Cross-check.** A shadow term that does not evaluate to an integer, or a
   record whose values fail the sum invariant, is an analysis error (exit 3),
   like other provenance failures in version 1. Shadow terms hold no pointers
   into the live operand, so a future rewrite of the live operand that the
   shadow does not reproduce shows up here as a failed evaluation or a failed
   sum, never as a stale reference.

Shadow terms exist only while instruction records are being captured and are
freed with the rest of the instruction provenance, so builds without records do
no extra work.

Redefinitions:

- A `=` symbol can be reassigned (`VALUE = 1` … `VALUE = 2`). After its first
  assignment, a use binds to the most recent assignment earlier in assembly
  order, which can differ from file position across includes and macros. A use
  that comes before the first assignment in assembly order is substituted only
  in `translate_instruction`, when the symbol holds its last assignment, so it
  assembles that value and binds to that assignment:

  ```asm
          LDA #LATER     ; A9 06, binds to LATER = 6
  LATER = 5
          LDA #LATER     ; A9 05, binds to LATER = 5
  LATER = 6
          LDA #LATER     ; A9 06, binds to LATER = 6
  ```

  `process_assign` currently gives a reassigned definition the first
  assignment's location. That stays for the xref `symbols` entry, and the
  producer keeps each assignment's own span separately for bindings.
- An `.equ` symbol cannot change: an identical redefinition is ignored, and a
  non-identical one is ignored with a warning. Its binding is always the first
  definition.

### Invariants

- Applying `projection` to `Σ sign × value`, then the operand-width reduction
  xasm applies to `operand_value`, yields `operand_value`. That reduction is
  `translate_instruction`'s truncation of an operand that is a constant after
  the translate stage: to a byte for immediate, zero-page and pointer modes
  when the value is outside -128 to 255, and to a word for absolute and
  indirect modes when it is outside 0 to $FFFF. Relative branches compare
  against the target before branch encoding.
- Term values never come from a number-to-label match; a term is only what was
  written.
- Decomposition is purely structural. It does not say which term is a base and
  which is a displacement; consumers apply their own policy, for example by
  choosing the one term bound to a constant.

## Non-goals

No control-flow, register-value, pointer-target, bank-visibility or stack
modelling. `memory_access.data.address` is null exactly when the address depends
on a pointer's runtime contents. Macro provenance rules are unchanged from
version 1.

Dummy bus accesses are not modelled: the write of the unmodified value that a
read-modify-write instruction makes before its final write, and the extra read
that indexed and pointer-indexed modes make at a partially computed address.
Analyses of registers with read or write side effects must account for them
separately.

## Documentation

Update with the implementation:

- `XASM_INSTRUCTION_RECORDS_SPEC.md`: marked as superseded by this spec.
- `XASM_REVERSE_ENGINEERING_FEATURES_SPEC.md`: the reference access values,
  defining `address_compute` and `immediate` (no document defines
  `address_compute` today), summary counting and ranking,
  `indirect_data_flows.access_kind`, index-pattern `access_kind` and
  `write_access`, and data-consumer site lists.
- `XASM_ANALYSIS_FEATURES_GUIDE.md`: the same, including its summary and
  index-pattern examples.

## Implementation order

1. The shared classifier and `memory_access`, with the legacy output changes.
   This part is mostly mechanical and causes most of the output changes.
2. `additive_terms` through the shadow reduction.

The two stages ship as one change, because the single version bump covers both
fields.

## Verification

`tests/test_instruction_records.py` covers the records and
`tests/test_access_classification.py`, run by `tests/regression.sh`, the legacy
outputs:

- Every record of every fixture, including the coverage fixtures, checked
  against an independent table of the 151 official opcodes for
  `memory_access`, and for the sum invariant.
- Every official opcode and mode, including `BIT` as a read and `JMP [addr]`
  with a pointer and no data.
- Every row of the reference access table, including `JMP [addr]` and
  `STA [zp],Y` as reads of the pointer, an immediate label without a projection
  as `immediate`, `.DW Handler` still as `address_compute` with a null `opcode`,
  and `LDA #PTR_LO` with `PTR_LO .EQU <Table` as `pointer_lo`. The
  default-output check of access values in `tests/regression.sh` gains
  `read_modify_write` and `immediate`.
- Read-modify-write in every output: one site in both `data_reads` and
  `data_writes`; summary read and write counts; an `INC Table,X` index-pattern
  record with `access_kind` `read_modify_write` and the `write_access` flag
  that forms no paired or split pattern; and the site in both data-consumer
  lists.
- A label read only by `BIT` and a pointer read only by `JMP [addr]` appearing
  in `top_data_labels`, not `top_jump_targets`, and `CMP` direct, indexed and
  immediate operands in every legacy output.
- Pointer-pair tracking: stores to `ptr` and `ptr+1`, a `[ptr],Y` access,
  `INC ptr+1`, then a second `[ptr],Y` access. Both accesses have flows from
  the original stores.
- Pointer high bytes for `[zp],Y` and `[zp,X]` at `$FF`, and `JMP [$12FF]`
  reading its high byte from `$1200`.
- Additive terms for `sym + sym`, `sym - (a + b)`, `-(a - 2)`,
  `sym + (A * 2) + B`, low and high projections, `Enum::Member`, a bare enum
  member (`enum_member` binding), `defined[X]` for a symbol defined later (1)
  and one never defined (0), local labels, backward and forward anonymous
  labels at two levels (`+`, `++`) including forward references inside a macro
  expansion and a `.REPT` block, a macro argument used twice in one operand,
  and `current_pc`.
- Bindings for a constant defined as `Label + 1` before the label, `A = B + 1`
  with `B` assigned after the use (the term binds to `A`), aliases of a later
  constant and of a label (the term binds to the alias), a constant reassigned
  with `=` between two instructions (different values and definition spans), a
  use before a reassigned constant's first assignment (binds to the last
  assignment, the value assembled), a non-identical `.equ` redefinition
  (binding stays the first definition), and a command-line define (null
  definition).
- Shadow side effects: identical stderr, exit status, binary and legacy xref
  with records captured and not captured for every fixture, including a source
  whose `sizeof` and division errors must each print once.
- Cost: the existing performance tests, and a large real disassembly kept
  outside the repository, timed with records captured and not captured.
- Binary and warning parity with version 1 output, and legacy xref, summary,
  index-pattern and data-consumer output that differs from a version 1 build
  only by the classification changes under
  [Shared classifier](#shared-classifier), on a fixture that exercises each
  change.
