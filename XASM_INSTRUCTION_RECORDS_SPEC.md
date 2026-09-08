# Structured instruction records

Status: version 1 producer implementation. This is a general assembler-fact
interface, not a policy-specific raw-address or branch-literal audit.

## Invocation and compatibility

```sh
xasm --pure-binary --xref=analysis.json --xref-format=json \
  --xref-instructions=true input.asm -o output.bin
```

The option defaults to false. Version 1 requires pure-binary output and JSON
xref; incompatible options fail with CLI exit 2. Object-mode unresolved
relocations are deliberately outside this first version. The section can be
produced together with listings, data xref, and index-pattern outputs in one
assembler invocation.

The existing xref document version remains `"2"`. The optional independently
versioned section is:

```json
{"instruction_records":{"version":"1","records":[]}}
```

Without the option, that section is absent, not an empty claim of coverage.
With the option, a successful assembly includes every emitted instruction in
output order, including literal-only, implied, and accumulator instructions.
Data and storage directives are not instructions. A valid assembly containing
only data has an empty stream. Invalid input, unavailable provenance, and
output errors cannot certify a successful stream. As with other xasm outputs,
callers must disregard artifacts from unsuccessful invocations; an existing or
partially written output file does not establish success.

Local/anonymous symbol filters still govern existing xref sections. They do not
filter this complete instruction stream or its pre-fold symbolic spellings.
Debug mode does not change the stream. Enabling the option must preserve binary
output, warnings, and existing xref sections apart from normal build metadata.

## Record contract

| Field | Meaning |
|---|---|
| `origin_id` | Positive encounter ID assigned before operand folding; retained through later passes. Unique within this assembly, not a persistent ID across edits or builds. |
| `use` | Source span of this use/expansion; macro invocations may share a span. |
| `source` | Parsed instruction span and exact text, before macro substitution. |
| `operand_source` | Parsed operand span/text, including `#`, grouping, indirection delimiters and indexing; null for implied mode. |
| `lexical_owner` | Preceding global label in the current code/ORG segment, or null. A lexical boundary, not inferred procedure ownership. |
| `segment_id` | Assembly-local segment identity; CPU addresses alone are not unique across ORG/bank regions. |
| `cpu_address`, `output_offset` | Integer CPU address and byte offset into this pure-binary output. |
| `opcode`, `mnemonic`, `size`, `bytes` | Final opcode byte, canonical uppercase mnemonic, emitted length, and complete emitted bytes. |
| `addressing_mode` | Final mode decoded from the emitted opcode, including late zero-page shortening. |
| `parsed_addressing_mode` | Parser mode before optimization; branches initially parse as absolute. |
| `immediate`, `index_register` | Boolean immediate mode and `"X"`, `"Y"`, or null. Postindexed indirection reports Y, not indexing of the pointer bytes. |
| `operand_form` | `integer_literal`, `symbol`, `expression`, or `none`, classified before folding. Accumulator syntax has no expression. |
| `expression` | Typed pre-fold expression after macro argument substitution, or null. This is the normalized expression representation; consumers must not parse display strings to recover it. |
| `referenced_symbols` | Ordered, unique pre-fold symbolic spellings. Not necessarily keys into the filtered final xref symbol table. |
| `structural_base` | Restricted written base-symbol/displacement/projection relationship, or null; see below. |
| `operand_value` | Value evaluated from the final operand at this instruction's PC, after assembler reductions/truncations, before branch-relative encoding; null for operandless instructions. |
| `branch_displacement` | Assembler-computed target minus next instruction address for relative mode, otherwise null. Emitted `bytes` remain authoritative for encoded behavior. |

Modes are `implied`, `accumulator`, `immediate`, `zeropage`, `zeropage_x`,
`zeropage_y`, `absolute`, `absolute_x`, `absolute_y`, `preindexed_indirect`,
`postindexed_indirect`, `indirect`, and `relative`.

Every span has `file`, `line`, `column`, `end_line`, and `end_column`.
Positions are one-based; the end is exclusive. Columns count source bytes, with
a tab counting as one byte. Paths identify resolved input locations, including
same-basename files in different include directories. They are not promised to
be symlink-canonical paths. A source object contains `span` and `text`.
Instruction and operand spans come from the grammar, not a second source parser.
Source bytes are retained when their input files are opened, without the old
listing renderer's fixed line-length limit.

For a macro, `source` and `operand_source` describe the definition's template;
they must not be presented as the expanded operand spelling. `use` identifies
the invocation. Expression nodes retain their individual parsed source spans,
so substituted argument literals retain their invocation spelling and radix.
Repeated/nested expansions can share source/use spans but have distinct IDs and
output offsets. There is no claim of a full expansion call stack in version 1.

## Expression and structural-base semantics

Each expression node has `kind`, `source`, and ordered `children`. Additional
fields are `value` for integers, `name` for symbols/strings/datatypes, and
`operator` for operators. Kinds are:

- `integer`, `string`, `symbol`, `local_symbol`, `forward_label`, `backward_label`
- `current_pc`, `operator`, `member`, `scope`, `index`, `sizeof`, `mask`, `datatype`

Binary operators use their grammar spellings: `+`, `-`, `*`, `/`, `%`, `&`, `|`,
`^`, `<<`, `>>`, `<`, `>`, `==`, `!=`, `<=`, `>=`. Unary operators are
`bit_not`, `logical_not`, `low_byte`, `high_byte`, `negate`, and `bank`.
Datatype names are `byte`, `char`, `word`, `dword`, and `user`.

The tree preserves syntactic distinctions before folding: a named constant is
not a raw integer merely because both resolve to the same value. Parentheses
are represented by tree structure; exact grouping remains in `operand_source`.
Local/anonymous names are captured before subsequent namespace resolution;
macro namespace rewriting that already occurred is retained. Neither the names
nor source text are a stable cross-build symbol ID.

`structural_base` recognizes a plain identifier, optionally followed by `+` or
`-` and an integer node, optionally enclosed in low/high-byte projection. It
contains `symbol`, signed `displacement`, and `projection` (`none`, `low`,
`high`). It describes written syntax only: it does not establish the identity,
kind, value, lifetime, or alias expansion of that symbol's definition. It never
chooses a label because a number matches its address. Complex offsets,
multi-symbol arithmetic, local/anonymous references, member/index expressions,
and unsupported forms produce null; the expression tree is still available.
In particular, consumers must not bind a mutable/redefined name to the final
symbol table and claim that was its definition at an earlier instruction.

These records do not prove control flow, register liveness, pointer ownership,
or bank visibility. `operand_value` for an indirect instruction is the pointer
location, not the pointed-to address. Consumers retain their policy and evidence
limits; additional binding/dataflow facts require explicit producer extensions.

## Shared analysis bundle boundary

This producer section is usable in the same assembly as existing analysis
outputs, but **is not a freshness certificate**. Source snapshots here support
provenance; they are not a complete content-hashed dependency manifest. No
NESrev gate or standalone fallback is changed by this producer unit.

The follow-on invocation-local bundle must:

1. Have xasm enumerate and content-hash the inputs actually consumed: root and
   transitive source files, binary includes, character maps, and any other
   assembler input. Use the producer's file-resolution/read paths, not an
   include-directive regex. Identify effective options/defines/include paths and
   producer build. The source snapshot plumbing here is only one part of that
   work; binary/character-map input tracking and hashes are not implemented yet.
2. Bind successful outputs to those input hashes, producer/schema versions,
   project configuration, and address/bank domain. The wrapper adds hashes for
   its configuration and authored policy inputs; xasm does not own NESrev policy.
3. Detect source/dependency changes during production and reuse, including
   same-size changes preserving timestamps. Publish a completed descriptor only
   after producer success, output hashing, and consistent-input validation.
   Input/output existence or mtime alone is insufficient.
4. Refuse any supplied missing, malformed, partial, stale, mismatched, or
   incompatible bundle. A separately tested no-bundle standalone mode may
   generate fresh facts once; invalid supplied input must not trigger fallback.

Only then should the first post-assembly branch-literal consumer switch from
its old parser. Keep the bundle invocation-local, not a persistent cache.
Produce the union of required outputs in the normal wrapper's single assembly;
retain separately justified failure-only diagnostic assembly. Review changes
in coverage for macros, same-line instructions, inactive code, and lexical
policies explicitly rather than silently accepting different KPI totals.

## Verification

`python3 tests/test_instruction_records.py ./xasm` exercises record coverage,
all addressing-mode families, final zero-page shortening, operand spelling,
macros/repeats, include identity, same-line and long-line source spans,
debug/filter invariance, mutable operands, inactive code, conservative base
refusal, empty streams, CLI/assembly/output failures, and binary/warning/legacy
xref parity. It runs from `sh tests/regression.sh` as well.

Generated parser/scanner files are checked in for ordinary builds. Regenerate
with Bison 3.8.2 and Flex 2.6.4 after grammar/lexer changes. This work also fixes
the opcode-mode lookup for STX/LDX zero-page-Y and LDX absolute-Y; the independent
opcode-byte fixtures cover those three pre-existing misclassifications.
