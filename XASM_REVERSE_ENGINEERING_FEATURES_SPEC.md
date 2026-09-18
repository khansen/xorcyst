# xasm Analysis Features Spec

## Status

Tier 1 (`--xref-summary`, `--xref-data`, `--analyze-index-patterns`) and
Tier 2 (`--data-consumers`, `--analyze-data-coverage`) are implemented and
shipped; field names, defaults, and behavior in this document match `xasm`'s
actual CLI and JSON output for those two tiers as of this writing. Tier 3
(`--analyze-dispatch-prefix`, `--warn-structure-mismatch`, `--format-hints`,
Section 5.6-5.8) is proposed design only — none of the three has been
implemented. Section 9 (Exit Codes) has been updated to match the shipped
per-feature scheme; treat every other section as the authoritative contract
for Tier 1/2 and a design proposal for Tier 3.

## 1. Purpose

Define optional post-assembly analysis features for `xasm` that inspect symbol usage,
data access patterns, and declaration structure without modifying source.

The features in this spec are intentionally general-purpose. They must not assume
any repository-specific naming convention, workflow, or reverse-engineering process.

## 2. Design Principles

1. Analysis features are additive.
- Existing assembly and output behavior must remain unchanged unless an explicit
  analysis flag is passed.

2. Outputs must be based on explicit, mechanical evidence.
- Prefer concrete counters and observed instruction patterns over heuristic
  "confidence" fields.

3. Features must degrade conservatively.
- If an analysis cannot prove a stronger claim, it must omit the field or mark
  the result as `unknown` rather than guessing.

4. Every feature must define:
- input prerequisites
- output destination behavior
- address encoding
- sort order
- record schema
- interaction with other analysis flags

## 3. Analysis Model

This section defines the internal concepts used by the feature set. These are
new analysis layers; they do not exist implicitly today.

### 3.1 Analysis Pipeline

Post-assembly analysis is defined as four layers:

1. `instruction model`
- Linear decoded instruction stream with:
  - CPU address
  - opcode bytes
  - addressing mode
  - referenced symbol/expression if present

2. `symbol model`
- Non-local symbol table with:
  - name
  - CPU address
  - symbol kind (`code`, `data`, `unknown`)
  - local/anonymous classification

3. `reference model`
- Direct reference edges from instructions to symbols:
  - call (`JSR`)
  - jump (`JMP`, branches)
  - data read
  - data write
- This is the minimum substrate required by `--xref`, `--xref-summary`, and the
  direct-read portion of `--xref-data`.

4. `derived data analysis model`
- Optional higher-level facts derived from the instruction and reference models:
  - eligible data spans
  - observed indexed access patterns
  - aggregated access coverage
  - prefix/offset dispatch structures
- This layer is required by the span- and structure-oriented features.

### 3.2 Eligible Data Symbols and Spans

Span-based features require a conservative definition of data-symbol span.

A symbol is an `eligible data symbol` when all are true:
- it is non-local
- its address is emitted by a data directive (`.DB`, `.DW`, or equivalent raw
  data emission directive)
- it is the first non-local symbol at that address

Primary span rules:
- `start` = symbol address
- `end_exclusive` = earliest of:
  - next non-local symbol address greater than `start`
  - next `.ORG` boundary
  - end of section/bank
- span membership is byte-oriented

Overlap rules:
- A later symbol inside an earlier primary span is an `overlap symbol`
- overlap symbols are valid symbols, but they do not implicitly redefine the
  earlier symbol's primary span
- span-based analyzers exclude overlap symbols by default unless an explicit
  `--include-overlaps=true` flag is provided

Code labels:
- code labels do not participate in span-based analyzers unless a feature
  explicitly says otherwise

### 3.3 Routine Ownership

Features that emit a `routine` field must use a syntactic ownership rule rather
than inferred procedure boundaries.

Routine owner rule:
- the owner is the nearest preceding non-local code label in the same section
  whose address is less than or equal to `site_addr`
- if no such label exists, omit the `routine` field

Important limitation:
- ownership is purely lexical; it does not imply that control can actually
  reach the site from the label
- bytes after `RTS`/`RTI`/`JMP` but before the next non-local code label still
  belong to the same lexical routine owner

### 3.4 Displacement Semantics

For features that report `displacement`, the value is the signed constant
offset relative to the referenced base symbol before register indexing.

Examples:
- `LDA Table,X` -> `displacement = 0`
- `LDA Table+3,X` -> `displacement = 3`
- `LDA Table-1,Y` -> `displacement = -1`
- `STA Table+2` -> `displacement = 2`

The displacement field does not include the runtime value of `X` or `Y`.

### 3.5 Address Encoding

Unless a feature says otherwise:
- JSON/NDJSON addresses are emitted as lowercase `0x`-prefixed hex strings
- text output may also use `0x`-prefixed hex strings
- existing `--xref` output remains backward compatible if already implemented
  differently; the new features do not retroactively redefine legacy output

### 3.6 Output Destinations

Every new feature follows this rule unless overridden:
- if `--<feature>-output=PATH` is provided, write to that file
- otherwise write to stdout
- if stdout is selected, only that feature's payload is emitted
- multiple analysis features may be requested in one invocation only when each
  has an explicit output path

### 3.7 Coverage Semantics

Span-based coverage features report only byte-exact coverage.

Exact coverage contributions:
- unindexed direct accesses to `Symbol` or `Symbol+const`
- indexed accesses only when the effective constant offset can be resolved in a
  local window:
  - same basic block
  - within the previous 3 instructions
  - immediate load into the active index register
  - no intervening clobber of that register

All other indexed accesses:
- contribute to read/write counts and access-site lists
- do not contribute to `covered_ranges`
- set `has_indexed_accesses_without_exact_coverage=true` on the aggregate record

Coverage byte width:
- for the purposes of these features, each 6502 memory read/write contributes a
  one-byte exact access range
- adjacent exact access bytes are merged into contiguous ranges

### 3.8 Implementation Tiers

The features are grouped by dependency depth.

Tier 1: direct reference and local-pattern analysis
- `--xref-summary`
- `--xref-data`
- `--analyze-index-patterns`

Tier 2: span aggregation
- `--data-consumers`
- `--analyze-data-coverage`

Tier 3: higher-level structural inference
- `--analyze-dispatch-prefix`
- `--warn-structure-mismatch`
- `--format-hints`

The spec is implementable incrementally by tier.

## 4. Common Conventions

### 4.1 Supported Formats

Unless stated otherwise, features support:
- `json`
- `ndjson`
- `text`

### 4.2 Sort Order

Unless stated otherwise:
- JSON arrays are sorted by the feature's primary address key ascending
- NDJSON records are emitted in the same order
- text output follows the same logical ordering

### 4.3 Local and Anonymous Symbols

Features that summarize symbols exclude local and anonymous labels by default.

Inclusion flags:
- `--include-locals=true|false`
- `--include-anon=true|false`

If a feature needs narrower flag names for compatibility, it may prefix them
with the feature name, but semantics must remain identical.

## 5. Feature Specifications

## 5.1 `--xref-summary` (Tier 1)

Command:

```sh
xasm --pure-binary \
  --xref-summary \
  --xref-summary-output=Game.xref_summary.json \
  --xref-summary-format=json \
  Game.asm
```

Behavior:
- Produces a compact symbol-summary view derived from the direct reference model.
- Summarizes code-entry, jump-target, and data-symbol reference counts without
  requiring full span analysis.

Defaults:
- `--xref-summary-format=json`
- `--xref-summary-limit=25`
- `--xref-summary-top-referrers=5`
- `--xref-summary-nearby-window=128`
- `--include-locals=false`
- `--include-anon=false`
- `--xref-summary-kind=all`
- ranking:
  - `top_callables`: sort by `jsr_count` desc, then `total_ref_count` desc, then address asc
  - `top_jump_targets`: sort by `jmp_count + branch_count` desc, then address asc
  - `top_data_labels`: sort by `read_count + write_count` desc, then address asc

Options:
- `--xref-summary-output=PATH`
- `--xref-summary-format=json|ndjson|text`
- `--xref-summary-kind=callable|jump_target|data|all`
- `--xref-summary-limit=N`
- `--xref-summary-top-referrers=N`
- `--xref-summary-nearby-window=BYTES`
- `--xref-summary-include=REGEX`
- `--xref-summary-exclude=REGEX`
- `--include-locals=true|false`
- `--include-anon=true|false`

Field semantics:
- `first_run_terminator`:
  - `rts`, `rti`, or `jmp` only when that opcode terminates the symbol's first
    contiguous instruction run before the next non-local symbol
  - `fallthrough` when decoding reaches the next non-local symbol without a
    terminating instruction
  - `unknown` when decoding cannot classify the symbol conservatively
- `next_symbol_distance_bytes`:
  - byte distance to the next non-local symbol
  - omitted when the next non-local symbol is not in the same section
- `nearby_symbols`:
  - non-local symbols within `± nearby_window` bytes of the anchor symbol,
    sorted by absolute address distance

Output sections:
- `top_callables`
- `top_jump_targets`
- `top_data_labels`

`top_callables` record:
- `label`
- `addr`
- `jsr_count`
- `jmp_count`
- `total_ref_count`
- `top_referring_routines` (`[{routine, count}]`)
- `first_run_terminator`
- `next_symbol_distance_bytes` (optional)
- `has_refs_from_other_routines`
- `nearby_symbols` (optional)

`top_jump_targets` record:
- `label`
- `addr`
- `jmp_count`
- `branch_count`
- `total_ref_count`
- `top_referring_routines` (`[{routine, count}]`)
- `first_run_terminator`
- `next_symbol_distance_bytes` (optional)
- `has_refs_from_other_routines`
- `nearby_symbols` (optional)

`top_data_labels` record:
- `label`
- `addr`
- `read_count`
- `write_count`
- `total_ref_count`
- `top_referring_routines` (`[{routine, count}]`)
- `next_symbol_distance_bytes` (optional)
- `has_refs_from_other_routines`
- `nearby_symbols` (optional)

JSON example:

```json
{
  "top_callables": [
    {
      "label": "UpdateEntityState",
      "addr": "0xc632",
      "jsr_count": 4,
      "jmp_count": 1,
      "total_ref_count": 5,
      "top_referring_routines": [
        {"routine": "ProcessEntities", "count": 2},
        {"routine": "HandleCollision", "count": 1}
      ],
      "first_run_terminator": "rts",
      "next_symbol_distance_bytes": 26,
      "has_refs_from_other_routines": true,
      "nearby_symbols": ["ProcessEntities", "SaveEntity"]
    }
  ],
  "top_jump_targets": [
    {
      "label": "EntityLoopContinue",
      "addr": "0xc65a",
      "jmp_count": 2,
      "branch_count": 3,
      "total_ref_count": 5,
      "top_referring_routines": [
        {"routine": "ProcessEntities", "count": 5}
      ],
      "first_run_terminator": "fallthrough",
      "next_symbol_distance_bytes": 12,
      "has_refs_from_other_routines": false,
      "nearby_symbols": ["UpdateEntityState"]
    }
  ],
  "top_data_labels": [
    {
      "label": "VelocityTable",
      "addr": "0xdf20",
      "read_count": 6,
      "write_count": 0,
      "total_ref_count": 6,
      "top_referring_routines": [
        {"routine": "ApplyVelocity", "count": 4},
        {"routine": "InitVelocity", "count": 2}
      ],
      "next_symbol_distance_bytes": 16,
      "has_refs_from_other_routines": true,
      "nearby_symbols": ["AccelerationTable"]
    }
  ]
}
```

NDJSON example:

```jsonl
{"section":"top_callables","label":"UpdateEntityState","addr":"0xc632","jsr_count":4,"jmp_count":1,"total_ref_count":5,"top_referring_routines":[{"routine":"ProcessEntities","count":2}],"first_run_terminator":"rts","next_symbol_distance_bytes":26,"has_refs_from_other_routines":true,"nearby_symbols":["ProcessEntities","SaveEntity"]}
{"section":"top_jump_targets","label":"EntityLoopContinue","addr":"0xc65a","jmp_count":2,"branch_count":3,"total_ref_count":5,"top_referring_routines":[{"routine":"ProcessEntities","count":5}],"first_run_terminator":"fallthrough","next_symbol_distance_bytes":12,"has_refs_from_other_routines":false,"nearby_symbols":["UpdateEntityState"]}
{"section":"top_data_labels","label":"VelocityTable","addr":"0xdf20","read_count":6,"write_count":0,"total_ref_count":6,"top_referring_routines":[{"routine":"ApplyVelocity","count":4}],"next_symbol_distance_bytes":16,"has_refs_from_other_routines":true,"nearby_symbols":["AccelerationTable"]}
```

Text example:

```text
top_callables
  UpdateEntityState @ 0xc632 jsr=4 jmp=1 total=5 term=rts size=26 external=true
    referrers: ProcessEntities(2), HandleCollision(1)
    nearby: ProcessEntities, SaveEntity

top_jump_targets
  EntityLoopContinue @ 0xc65a jmp=2 branch=3 total=5 term=fallthrough size=12 external=false
    referrers: ProcessEntities(5)
    nearby: UpdateEntityState

top_data_labels
  VelocityTable @ 0xdf20 read=6 write=0 total=6 size=16 external=true
    referrers: ApplyVelocity(4), InitVelocity(2)
    nearby: AccelerationTable
```

## 5.2 `--xref-data` (Tier 1)

Command:

```sh
xasm --pure-binary \
  --xref=Game.xref.json \
  --xref-format=json \
  --xref-include-owner=true \
  --xref-data=true \
  Game.asm
```

Behavior:
- Extends existing xref output with direct data-read and data-write edges.
- Optional indirect-flow reporting is conservative and limited to same-routine,
  same-pointer-pair producer/consumer matches.
- `--xref-include-owner=true` adds lexical owner fields to ordinary `references`
  records and to the `data_reads` / `data_writes` / `indirect_data_flows`
  records described in this section.

Defaults:
- `--xref-data=false`
- `--xref-include-owner=false`
- when `--xref-data=true`, new keys are merged into the normal xref payload
- duplicate edges are coalesced by
  `(symbol, site_addr, owner_routine, displacement, addressing_mode)`
- initial implementation requires `--xref-format=json`

Format compatibility:
- `--xref-data=true` with `--xref-format=json` is valid
- `--xref-data=true` with `--xref-format=text` or `csv` must fail with CLI
  validation error until those formats gain an explicit extension design
- `--xref-include-owner=true` with `--xref-format=json` is valid
- `--xref-include-owner=true` with `--xref-format=text` or `csv` is reserved
  for future extension design and must fail with CLI validation error in the
  initial implementation
- `--xref-include-owner=true` without `--xref=...` is invalid and must fail
  with CLI validation error in the initial implementation

Lexical owner fields:
- `owner_routine`
- `owner_routine_addr`
- lexical owner uses the shared routine-owner rule from Section 3.3
- the owner fields are omitted when no such lexical owner exists
- owner fields are a convenience view over existing symbol/line information;
  they do not imply control-flow reachability or callable semantics
- `owner_routine_addr` must use the standard new-feature address encoding from
  Section 3.5: lowercase `0x`-prefixed hex

Base `references` extension when `--xref-include-owner=true`:
- each emitted `references` record must include:
  - `owner_routine`
  - `owner_routine_addr`
- when a lexical owner exists
- records with no lexical owner must omit both owner fields
- this applies to all emitted `references` records, including instruction-based
  and directive/expression-based references

Scope limits:
- `data_reads` and `data_writes` include only direct references resolved at
  assembly time
- supported direct addressing forms:
  - `absolute`
  - `absolute_x`
  - `absolute_y`
  - `zeropage`
  - `zeropage_x`
  - `zeropage_y` if supported by the target CPU mode
- `indirect_data_flows` is emitted only when:
  - a zero-page pointer pair is written in one routine
  - an indirect read/write through that same pair occurs later in the same routine
  - no intervening write to either byte of that same pointer pair invalidates
    the match
- indirect-indexed consumer forms allowed:
  - `[ptr],Y`
  - `[ptr,X]` if supported by the target CPU mode
- one indirect-flow record is emitted per consumer site
- indirect-flow duplicates are coalesced by
  `(ptr_symbol, producer_site, consumer_site, access_kind, owner_routine)`

Pointer-pair identification:
- the pointer pair is identified by two consecutive zero-page byte addresses
- producer matching requires both bytes of the pair to be written after the most
  recent invalidation point and before the consumer site
- low/high byte write order does not matter
- if both bytes have symbol names and exactly one symbol names the pair as a
  whole, emit that symbol
- otherwise emit the lower-byte symbol when present
- otherwise omit the indirect-flow record rather than invent a synthetic name

New keys:
- `data_reads`
- `data_writes`
- `indirect_data_flows`

`data_reads` / `data_writes` record:
- `symbol`
- `site_addr`
- `routine` (optional)
- `owner_routine` (optional)
- `owner_routine_addr` (optional)
- `displacement`
- `addressing_mode`

`indirect_data_flows` record:
- `ptr_symbol`
- `producer_site`
- `consumer_site`
- `access_kind` (`read|write`)
- `routine` (optional)
- `owner_routine` (optional)
- `owner_routine_addr` (optional)

Owner-field consistency:
- when `owner_routine` is present, `routine` may be omitted or may duplicate
  `owner_routine`
- new consumers should prefer `owner_routine`
- `routine` remains permitted for backward compatibility with earlier
  `--xref-data` payloads

JSON example:

```json
{
  "references": [
    {
      "symbol": "UpdateEntityState",
      "file": "Game.asm",
      "line": 122,
      "column": 5,
      "use_cpu_address": "0xc145",
      "opcode": "JSR",
      "addressing_mode": "absolute",
      "access": "call",
      "expression": "UpdateEntityState",
      "owner_routine": "ProcessEntities",
      "owner_routine_addr": "0xc130"
    }
  ],
  "data_reads": [
    {
      "symbol": "StateSelectorTable",
      "site_addr": "0xeaf7",
      "owner_routine": "UpdateStateBySelector",
      "owner_routine_addr": "0xeae0",
      "displacement": 0,
      "addressing_mode": "absolute_x"
    }
  ],
  "data_writes": [
    {
      "symbol": "StateBuffer",
      "site_addr": "0xc145",
      "owner_routine": "InitState",
      "owner_routine_addr": "0xc130",
      "displacement": 2,
      "addressing_mode": "absolute_y"
    }
  ],
  "indirect_data_flows": [
    {
      "ptr_symbol": "WorkPtr",
      "producer_site": "0xc120",
      "consumer_site": "0xc145",
      "access_kind": "read",
      "owner_routine": "InitState",
      "owner_routine_addr": "0xc130"
    }
  ]
}
```

NDJSON example:

```jsonl
{"section":"references","symbol":"UpdateEntityState","file":"Game.asm","line":122,"column":5,"use_cpu_address":"0xc145","opcode":"JSR","addressing_mode":"absolute","access":"call","expression":"UpdateEntityState","owner_routine":"ProcessEntities","owner_routine_addr":"0xc130"}
{"section":"data_reads","symbol":"StateSelectorTable","site_addr":"0xeaf7","owner_routine":"UpdateStateBySelector","owner_routine_addr":"0xeae0","displacement":0,"addressing_mode":"absolute_x"}
{"section":"data_writes","symbol":"StateBuffer","site_addr":"0xc145","owner_routine":"InitState","owner_routine_addr":"0xc130","displacement":2,"addressing_mode":"absolute_y"}
{"section":"indirect_data_flows","ptr_symbol":"WorkPtr","producer_site":"0xc120","consumer_site":"0xc145","access_kind":"read","owner_routine":"InitState","owner_routine_addr":"0xc130"}
```

## 5.3 `--analyze-index-patterns` (Tier 1)

Command:

```sh
xasm --pure-binary \
  --analyze-index-patterns \
  --index-patterns-output=Game.index.json \
  --index-patterns-format=json \
  Game.asm
```

Behavior:
- Detects local instruction-window indexing patterns for direct data accesses.
- This feature does not perform full reaching-definition analysis.
- Pattern detection is limited to compact windows immediately preceding the
  indexed access site.
- Emits one record per matched access site after applying the precedence rules
  below.

Defaults:
- `--index-patterns-format=json`
- analysis window: up to 6 instructions before the indexed access site within
  the same lexical routine owner
- backward scan terminates at the first of:
  - a non-local label
  - a local/anonymous label that is a branch target
  - a conditional branch instruction
  - a call instruction (`JSR`)
  - an unconditional control transfer (`JMP`, `RTS`, `RTI`)
- duplicate records are coalesced by `(table_label, routine, site_addr, access_pattern)`
- only non-local data labels are emitted by default; locals/anonymous labels
  may be included with the shared `--include-locals=true` /
  `--include-anon=true` flags

Supported access scope:
- reads and writes are both in scope
- supported addressing forms:
  - `absolute_x`
  - `absolute_y`
  - `zeropage_x`
  - `zeropage_y` if supported by the target CPU mode
- excluded from this feature:
  - indirect-indexed forms (`[ptr],Y`, `[ptr,X]`)
  - accesses whose base symbol cannot be resolved at assembly time

Recognized access patterns:
- `base`
  - `LDA Table,X`, `STA Table,Y`, etc. with displacement `0`
- `base_plus_const`
  - `LDA Table+1,X`, `STA Table+2,Y`, etc.
- `base_minus_const`
  - `LDA Table-1,Y`, `STA Table-2,X`, etc.
- `paired_byte_reads`
  - two reads from the same base with displacements that differ by 1 inside the
    same local instruction window
- `scaled_index_stride_2`
  - e.g. `ASL A ; TAX ; LDA Table,X`
- `scaled_index_stride_4`
  - e.g. `ASL A ; ASL A ; TAX ; LDA Table,X`
- `split_lo_hi_tables`
  - two reads in the same window from labels matched by a configurable naming
    convention such as `*Lo` / `*Hi`

Important limitation:
- address-distance heuristics for split low/high tables are deferred to a
  future span-aware revision; Tier 1 pattern matching is naming-based only

Pattern precedence:
- exactly one `access_pattern` is emitted per matched site
- precedence order:
  1. `split_lo_hi_tables`
  2. `paired_byte_reads`
  3. `scaled_index_stride_4`
  4. `scaled_index_stride_2`
  5. `base_minus_const`
  6. `base_plus_const`
  7. `base`
- `estimated_record_width` may still be emitted for the winning pattern when
  defined by the rules below

Split low/high naming defaults:
- default suffix pairs:
  - `Lo` / `Hi`
  - `Low` / `High`
  - `_lo` / `_hi`
  - `_low` / `_high`
- matching is case-sensitive
- both labels must share the same stem after removing the matched suffix
- optional override:
  - `--index-patterns-split-pairs=Lo:Hi,Low:High,_lo:_hi,_low:_high`

Matching rules for compound patterns:
- `paired_byte_reads`
  - both reads must:
    - use the same index register
    - have the same `index_value_source_kind`
    - target the same base label
    - be in the same local instruction window
- `split_lo_hi_tables`
  - emit a single record keyed by the shared stem in `table_label`
  - add:
    - `table_label_lo`
    - `table_label_hi`
  - `displacement` is the constant displacement used by the anchoring access
  - `site_addr` is the CPU address of the first matching low/high access in the pair

Field semantics:
- `index_value_source_kind`:
  - one of `register`, `immediate`, `scaled_accumulator`, `scaled_register`, `unknown`
  - this is intentionally mechanical; it is not a semantic value name
  - mapping rules:
    - `immediate`: active index register was loaded from `LDX #imm` / `LDY #imm`
      in the local instruction window with no intervening clobber
    - `register`: indexed access uses `X`/`Y` with no recognized scaling step in
      the local instruction window
    - `scaled_accumulator`: one or more `ASL A` operations feed a transfer into
      the active index register (`TAX`/`TAY`)
    - `scaled_register`: one or more `ASL` operations act directly on the active
      index register in the local instruction window
    - `unknown`: none of the above can be established conservatively
- `estimated_record_width`:
  - emitted only for scaled-index or tightly paired-byte patterns
  - for `paired_byte_reads`: `2`
  - for `scaled_index_stride_2`: `2`
  - for `scaled_index_stride_4`: `4`
  - non-zero displacement does not suppress `estimated_record_width`
- `site_addr`:
  - CPU address of the access instruction that anchors the emitted record
  - for paired patterns, this is the first instruction in the matched pair
- `access_kind`:
  - `read` when the anchoring instruction reads from the table symbol
  - `write` when the anchoring instruction writes to the table symbol
  - for paired read patterns, `access_kind` is `read`
- `routine`:
  - follows the shared routine-ownership rule in Section 3.3
  - omit the field if no lexical routine owner exists
- `evidence_flags`:
  - allowed values in the initial implementation:
    - `negative_displacement`
    - `adjacent_read_pair`
    - `scaled_index`
    - `split_named_lo_hi`
    - `write_access`

Record:
- `table_label`
- `routine` (optional)
- `site_addr`
- `access_kind`
- `access_pattern`
- `index_register`
- `displacement`
- `index_value_source_kind`
- `estimated_record_width` (optional)
- `table_label_lo` (optional, split low/high only)
- `table_label_hi` (optional, split low/high only)
- `evidence_flags` (`[flag,...]`, optional)

Output shape:
- `json`: array of records
- `ndjson`: one record per line
- `text`: one block per record

JSON example:

```json
[
  {
    "table_label": "FrequencyTable",
    "routine": "ApplyFrequency",
    "site_addr": "0xe8a1",
    "access_kind": "read",
    "access_pattern": "paired_byte_reads",
    "index_register": "Y",
    "displacement": -1,
    "index_value_source_kind": "register",
    "estimated_record_width": 2,
    "evidence_flags": ["negative_displacement", "adjacent_read_pair"]
  }
]
```

NDJSON example:

```jsonl
{"table_label":"FrequencyTable","routine":"ApplyFrequency","site_addr":"0xe8a1","access_kind":"read","access_pattern":"paired_byte_reads","index_register":"Y","displacement":-1,"index_value_source_kind":"register","estimated_record_width":2,"evidence_flags":["negative_displacement","adjacent_read_pair"]}
```

Text example:

```text
FrequencyTable read @ 0xe8a1 in ApplyFrequency
  pattern=paired_byte_reads index=Y displacement=-1 origin=register
  estimated_record_width=2
  evidence_flags=negative_displacement, adjacent_read_pair
```

Negative cases:
- a single `LDA Table,X` with no local evidence of pairing or scaling must not
  emit `estimated_record_width`
- two reads separated by a call or a new basic-block label must not be merged
  into one paired-byte record

## 5.4 `--data-consumers` (Tier 2)

Command:

```sh
xasm --pure-binary \
  --data-consumers \
  --data-consumers-output=Game.data_consumers.json \
  --data-consumers-format=json \
  Game.asm
```

Behavior:
- Aggregates direct data-access observations by eligible data symbol span.
- Summarizes which routines read or write the symbol, which displacements are
  observed, and which portion of the primary span is touched.

Defaults:
- `--data-consumers-format=json`
- overlap symbols excluded by default
- `--include-overlaps=true` enables emission for overlap symbols using the same
  shared rule defined in Section 3.2
- output ordering: ascending by symbol address
- `read_sites` and `write_sites` are ordered by `site_addr` ascending
- duplicate site records are coalesced by
  `(routine, site_addr, displacement, addressing_mode)`

Substrate behavior:
- `--data-consumers` is self-contained
- if direct read/write analysis has not already been computed internally for the
  current invocation, this feature must compute the required direct-access
  substrate itself
- indirect flows may contribute only when the implementation supports the
  constrained same-routine pointer-pair model defined for `--xref-data`

Record:
- `label`
- `declared_start`
- `declared_end_exclusive`
- `declared_size`
- `read_site_count`
- `write_site_count`
- `distinct_routine_count`
- `observed_constant_displacements` (`[int,...]`)
- `covered_ranges` (`[{start,end_exclusive}]`)
- `uncovered_ranges` (`[{start,end_exclusive}]`)
- `has_indexed_accesses_without_exact_coverage`
- `access_patterns` (`[string,...]`)
- `read_sites` (`[{routine, site_addr, displacement, addressing_mode}]`)
- `write_sites` (`[{routine, site_addr, displacement, addressing_mode}]`)

Notes:
- `json`: array of records
- `ndjson`: one record per line
- `text`: one block per record
- `covered_ranges` and `uncovered_ranges` are computed only from direct reads
  and direct writes unless an indirect flow is already present in the
  `--xref-data` substrate
- only byte-exact accesses contribute to `covered_ranges`; unresolved indexed
  accesses do not
- `observed_constant_displacements` includes all compile-time constant
  displacements observed in direct read/write sites for the symbol, regardless
  of whether those accesses contributed exact coverage
- `observed_constant_displacements` excludes accesses whose displacement cannot
  be resolved to a compile-time constant
- `distinct_routine_count` counts the union of routine owners across
  `read_sites` and `write_sites`
- sites without a lexical routine owner do not contribute to
  `distinct_routine_count`
- `access_patterns` reuses the normalized pattern vocabulary from
  `--analyze-index-patterns` when a site can be classified there; otherwise
  only `base`, `base_plus_const`, and `base_minus_const` may appear in the
  initial implementation
- this feature is an aggregate span view; it is not a superset of
  `--analyze-index-patterns`

JSON example:

```json
[
  {
    "label": "PackedEntryCoordTable",
    "declared_start": "0xdf1b",
    "declared_end_exclusive": "0xdf25",
    "declared_size": 10,
    "read_site_count": 3,
    "write_site_count": 0,
    "distinct_routine_count": 2,
    "observed_constant_displacements": [0, 1, 2, 3],
    "covered_ranges": [{"start":"0xdf1b","end_exclusive":"0xdf1f"}],
    "uncovered_ranges": [{"start":"0xdf1f","end_exclusive":"0xdf25"}],
    "has_indexed_accesses_without_exact_coverage": false,
    "access_patterns": ["base", "base_plus_const"],
    "read_sites": [
      {"routine":"ReadPackedEntries","site_addr":"0xd230","displacement":0,"addressing_mode":"absolute_y"}
    ],
    "write_sites": []
  }
]
```

NDJSON example:

```jsonl
{"label":"PackedEntryCoordTable","declared_start":"0xdf1b","declared_end_exclusive":"0xdf25","declared_size":10,"read_site_count":3,"write_site_count":0,"distinct_routine_count":2,"observed_constant_displacements":[0,1,2,3],"covered_ranges":[{"start":"0xdf1b","end_exclusive":"0xdf1f"}],"uncovered_ranges":[{"start":"0xdf1f","end_exclusive":"0xdf25"}],"has_indexed_accesses_without_exact_coverage":false,"access_patterns":["base","base_plus_const"],"read_sites":[{"routine":"ReadPackedEntries","site_addr":"0xd230","displacement":0,"addressing_mode":"absolute_y"}],"write_sites":[]}
```

Text example:

```text
PackedEntryCoordTable @ 0xdf1b size=10
  reads=3 writes=0 routines=2
  observed_constant_displacements: 0, 1, 2, 3
  covered_ranges: [0xdf1b,0xdf1f)
  uncovered_ranges: [0xdf1f,0xdf25)
  access_patterns: base, base_plus_const
```

## 5.5 `--analyze-data-coverage` (Tier 2)

Command:

```sh
xasm --pure-binary \
  --analyze-data-coverage \
  --data-coverage-output=Game.coverage.json \
  --data-coverage-format=json \
  Game.asm
```

Behavior:
- Reports span coverage only.
- This feature is intentionally narrower than `--data-consumers`; it exists for
  tooling that needs coverage metrics without the full access-site payload.

Defaults:
- `--data-coverage-format=json`
- overlap symbols excluded by default
- `--include-overlaps=true` enables emission for overlap symbols using the same
  shared rule defined in Section 3.2
- adjacent ranges merged before emission
- output ordering: ascending by symbol address

Substrate behavior:
- `--analyze-data-coverage` is self-contained
- if direct read/write analysis has not already been computed internally for the
  current invocation, this feature must compute the required direct-access
  substrate itself
- exact coverage is computed from direct reads and direct writes
- indirect flows may contribute only when the implementation supports the
  constrained same-routine pointer-pair model defined for `--xref-data`

Record:
- `label`
- `declared_start`
- `declared_end_exclusive`
- `declared_size`
- `covered_ranges`
- `covered_size`
- `uncovered_ranges`
- `uncovered_size`
- `access_count`
- `has_indexed_accesses_without_exact_coverage`

Notes:
- `json`: array of records
- `ndjson`: one record per line
- `text`: one block per record
- `access_count` counts the total number of direct read/write access sites
  contributing to the symbol record, using the same site deduplication basis as
  `--data-consumers`
- `access_count` includes accesses that did not contribute exact coverage
- `covered_size` and `uncovered_size` are byte counts derived from the merged
  ranges
- `covered_size + uncovered_size == declared_size`
- text output may omit redundant scalar fields when the same information is
  obvious from displayed ranges, but it must include `access_count` and
  `has_indexed_accesses_without_exact_coverage`

JSON example:

```json
[
  {
    "label": "PackedEntryCoordTable",
    "declared_start": "0xdf1b",
    "declared_end_exclusive": "0xdf25",
    "declared_size": 10,
    "covered_ranges": [{"start":"0xdf1b","end_exclusive":"0xdf1f"}],
    "covered_size": 4,
    "uncovered_ranges": [{"start":"0xdf1f","end_exclusive":"0xdf25"}],
    "uncovered_size": 6,
    "access_count": 3,
    "has_indexed_accesses_without_exact_coverage": false
  }
]
```

NDJSON example:

```jsonl
{"label":"PackedEntryCoordTable","declared_start":"0xdf1b","declared_end_exclusive":"0xdf25","declared_size":10,"covered_ranges":[{"start":"0xdf1b","end_exclusive":"0xdf1f"}],"covered_size":4,"uncovered_ranges":[{"start":"0xdf1f","end_exclusive":"0xdf25"}],"uncovered_size":6,"access_count":3,"has_indexed_accesses_without_exact_coverage":false}
```

Text example:

```text
PackedEntryCoordTable @ 0xdf1b size=10
  covered_ranges: [0xdf1b,0xdf1f)
  uncovered_ranges: [0xdf1f,0xdf25)
  access_count: 3
  has_indexed_accesses_without_exact_coverage: false
```

## 5.6 `--analyze-dispatch-prefix` (Tier 3)

Command:

```sh
xasm --pure-binary \
  --analyze-dispatch-prefix \
  --dispatch-prefix-output=Game.dispatch.json \
  --dispatch-prefix-format=json \
  Game.asm
```

Behavior:
- Detects compact prefix/offset data structures when a routine:
  1. reads an offset from a bounded prefix region
  2. adds that offset to a common table base
  3. uses the result as the start of a record or substructure

This feature is heuristic and conservative.

Defaults:
- `--dispatch-prefix-format=json`
- output ordering: ascending by `prefix_start`
- only emit records with at least two distinct observed offset entries

Minimum evidence threshold:
- at least two distinct offset values read from the same prefix region
- a common base address used after the offset is loaded
- at least one subsequent indexed or indirect access rooted at `base + offset`

Non-goals:
- it does not require monotonic offsets
- it does not attempt to classify token semantics

Record:
- `table_label`
- `prefix_start`
- `prefix_length`
- `offset_entry_count`
- `record_region_start`
- `record_stride` (optional)
- `record_size_bytes` (optional)
- `sites` (`[{routine, site_addr}]`)

JSON example:

```json
{
  "table_label": "CommandDispatchData",
  "prefix_start": "0xfa10",
  "prefix_length": 8,
  "offset_entry_count": 8,
  "record_region_start": "0xfa18",
  "record_size_bytes": 6,
  "sites": [{"routine":"DispatchCommand","site_addr":"0xe950"}]
}
```

NDJSON example:

```jsonl
{"table_label":"CommandDispatchData","prefix_start":"0xfa10","prefix_length":8,"offset_entry_count":8,"record_region_start":"0xfa18","record_size_bytes":6,"sites":[{"routine":"DispatchCommand","site_addr":"0xe950"}]}
```

Text example:

```text
CommandDispatchData
  prefix_start=0xfa10 prefix_length=8 offset_entry_count=8
  record_region_start=0xfa18 record_size_bytes=6
  sites: DispatchCommand@0xe950
```

## 5.7 `--warn-structure-mismatch` (Tier 3)

Command:

```sh
xasm --pure-binary --warn-structure-mismatch Game.asm
```

Behavior:
- Emits warnings when declared data spans conflict with observed access shape.
- This feature depends on the Tier 1 and Tier 2 substrates.

Defaults:
- `W2401`, `W2402`: warning
- `W2404`: advisory warning

Warning classes:
- `W2401`: odd-length paired-byte access against a declared data span
- `W2402`: declared span has uncovered trailing bytes after a bounded
  dispatch-prefix or compact-access region
- `W2403`: reserved for a future width-annotation extension and not part of the
  initial implementation scope
- `W2404`: declared span contains disjoint access regimes that may merit
  explicit sub-labels or section boundaries

Important limitation:
- `W2403` must not be implemented until a separate spec defines explicit width
  annotations
- this spec does not assume width from comments or source row grouping

Escalation:
- `--Werror=structure-mismatch` upgrades `W2401`, `W2402`, and `W2404` to fatal
- `W2403` remains inactive until the width-annotation extension exists

Example warnings:

```text
Game.asm:204: warning W2401: symbol 'WordLookupTable' has declared size 5 but is read as adjacent byte pairs
Game.asm:518: warning W2402: symbol 'DispatchData' has uncovered trailing bytes after the observed compact-access region ending at 0xdfa4
Game.asm:1402: warning W2404: symbol 'MixedDataBlock' has disjoint access regimes and may need sub-labels or section boundaries
```

## 5.8 `--format-hints` (Tier 3)

Command:

```sh
xasm --pure-binary \
  --format-hints \
  --format-hints-output=Game.hints.json \
  --format-hints-format=json \
  Game.asm
```

Behavior:
- Emits presentation hints derived from Tier 1-3 analyses.
- Passing `--format-hints` alone is valid; `xasm` must internally run any
  prerequisite analyses needed to produce the hints.

Defaults:
- `--format-hints-format=json`
- output ordering: ascending by symbol address
- only hints backed by at least one explicit evidence item are emitted

Hint generation rules:
- `record_rows`
  - emit when `--analyze-index-patterns` reports `estimated_record_width`
- `index_table`
  - emit when all observed accesses are direct reads with constant
    displacements and no writes are observed
- `mixed_sections`
  - emit when `--analyze-dispatch-prefix` produces a record for the same symbol
    or when `W2404` would apply
- `trailing_bytes`
  - emit when `--analyze-data-coverage` reports a non-empty uncovered range
    strictly after the highest exact covered byte and
    `has_indexed_accesses_without_exact_coverage=false`

Record:
- `label`
- `hint_type` (`record_rows`, `index_table`, `mixed_sections`, `trailing_bytes`)
- `recommended_row_width` (optional)
- `recommended_sections` (optional)
- `evidence` (`[{source, site_addr}]`)

JSON example:

```json
{
  "label": "PackedScriptData",
  "hint_type": "mixed_sections",
  "recommended_sections": [
    {"name":"section_0","start":"0xe539","end_exclusive":"0xe53c"},
    {"name":"section_1","start":"0xe53c","end_exclusive":"0xe541"}
  ],
  "evidence": [
    {"source":"analyze_dispatch_prefix","site_addr":"0xe530"}
  ]
}
```

NDJSON example:

```jsonl
{"label":"PackedScriptData","hint_type":"mixed_sections","recommended_sections":[{"name":"section_0","start":"0xe539","end_exclusive":"0xe53c"},{"name":"section_1","start":"0xe53c","end_exclusive":"0xe541"}],"evidence":[{"source":"analyze_dispatch_prefix","site_addr":"0xe530"}]}
```

Text example:

```text
PackedScriptData
  hint_type=mixed_sections
  recommended_sections:
    - section_0 [0xe539,0xe53c)
    - section_1 [0xe53c,0xe541)
  evidence: analyze_dispatch_prefix@0xe530
```

## 6. Fixture-Oriented Test Requirements

Each feature must ship with fixture-based tests.

Minimum fixture classes:
1. positive direct-reference fixture
2. positive indexed-access fixture
3. negative near-miss fixture
4. overlap-symbol fixture
5. multiple-ORG / section-boundary fixture
6. all-features-enabled consistency fixture

Each fixture must define:
- source asm
- expected assembled symbol addresses
- exact expected JSON output, or exact asserted subset for large payloads
- expected warning lines when warnings are the output surface

Minimal complete fixture example:

```asm
        .ORG $C000
ReadPair:
        LDY #$01
        LDA WordTable-1,Y
        STA $00
        LDA WordTable,Y
        STA $01
        RTS

WordTable:
        .DB $34,$12,$78,$56
```

Expected `--analyze-index-patterns` JSON subset:

```json
{
  "table_label": "WordTable",
  "routine": "ReadPair",
  "access_pattern": "paired_byte_reads",
  "index_register": "Y",
  "displacement": -1,
  "index_value_source_kind": "immediate",
  "estimated_record_width": 2
}
```

Additional complete fixture: `--data-consumers`

```asm
        .ORG $C000
ReadCoords:
        LDX #$00
        LDA CoordTable,X
        STA $00
        LDA CoordTable+1,X
        STA $01
        RTS

CoordTable:
        .DB $10,$20,$30,$40
```

Expected `--data-consumers` JSON subset:

```json
{
  "label": "CoordTable",
  "declared_start": "0xc008",
  "declared_end_exclusive": "0xc00c",
  "declared_size": 4,
  "read_site_count": 2,
  "write_site_count": 0,
  "distinct_routine_count": 1,
  "observed_constant_displacements": [0, 1],
  "covered_ranges": [{"start":"0xc008","end_exclusive":"0xc00a"}],
  "uncovered_ranges": [{"start":"0xc00a","end_exclusive":"0xc00c"}],
  "has_indexed_accesses_without_exact_coverage": false,
  "access_patterns": ["base", "base_plus_const"]
}
```

Additional complete fixture: `--analyze-dispatch-prefix`

```asm
        .ORG $C000
DispatchRecord:
        TAX
        LDA DispatchData,X
        STA $00
        LDA #<DispatchData
        CLC
        ADC $00
        STA $01
        LDA #>DispatchData
        ADC #$00
        STA $02
        LDY #$00
        LDA [$01],Y
        RTS

DispatchData:
        .DB $02,$05
        .DB $A0,$A1,$A2
        .DB $B0,$B1,$B2
```

Expected `--analyze-dispatch-prefix` JSON subset:

```json
{
  "table_label": "DispatchData",
  "prefix_start": "0xc011",
  "prefix_length": 2,
  "offset_entry_count": 2,
  "record_region_start": "0xc013",
  "sites": [
    {"routine":"DispatchRecord","site_addr":"0xc001"}
  ]
}
```

Pattern-specific fixture requirements:
- `--analyze-index-patterns`:
  - `LDA Table,X`
  - `LDA Table+1,X`
  - `LDA Table-1,Y`
  - `ASL A ; TAX ; LDA Table,X`
  - `ASL A ; ASL A ; TAX ; LDA Table,X`
  - a negative case where reads are separated by a call or basic-block boundary
- `--analyze-dispatch-prefix`:
  - positive compact prefix-offset structure
  - negative plain indexed table read that must not match
- span-based features:
  - disjoint labels
  - overlap labels
  - section boundary truncation

## 7. Non-Goals

This spec does not require:
- semantic naming of symbols
- decompiler-style value naming
- full global dataflow or SSA
- automatic source rewriting
- guessing structure from comments alone
- inferring record width from source row formatting alone

## 8. Recommended Implementation Order

1. `--xref-summary`
2. `--xref-data` direct reads/writes
3. `--analyze-index-patterns`
4. eligible data spans
5. `--data-consumers`
6. `--analyze-data-coverage`
7. `--analyze-dispatch-prefix`
8. `--warn-structure-mismatch`
9. `--format-hints`

This order follows the dependency model and keeps early implementations testable
without speculative heuristics.

## 9. Exit Codes

Each Tier 1/2 feature has its own dedicated failure exit code, assigned in
implementation order rather than grouped under one generic code:
- `6`: `--xref-summary` failure
- `7`: `--analyze-index-patterns` failure
- `8`: `--data-consumers` failure
- `9`: `--analyze-data-coverage` failure

These run in the fixed sequence xref-summary, index-patterns, data-consumers,
data-coverage, and each stage is skipped once an earlier one has set a
nonzero exit code (`xasm.c`). A combined invocation requesting more than one
of these therefore reports the exit code of the *first* one to fail, not a
composite of every failure.

Codes shared with the rest of `xasm`, not specific to this spec:
- `2`: invalid CLI flag combinations or incompatible output-destination
  combinations (the general CLI-validation error class)
- `3`: destination validation or listing/xref/instruction-record/NL/manifest
  failure, and comparison/audit I/O errors
- `4`: `--audit-raw-addresses` findings promoted to fatal by
  `--audit-level=error`. `--audit-raw-addresses` is a separate, already-shipped
  feature outside this spec's Tier system; it reuses this exit code rather
  than `6`-`9`.
- `5`: `--compare` mismatch

A future `--Werror=structure-mismatch` (Tier 3, not implemented) would be
expected to reuse exit code `4` the same way `--audit-level=error` does, but
that is a design intent for Section 5.7, not current behavior.
