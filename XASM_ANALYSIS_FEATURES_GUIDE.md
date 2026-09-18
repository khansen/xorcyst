# xasm Reverse-Engineering Analysis Features: A Worked Guide

## What this document is

`xasm` includes a set of features aimed at reverse-engineering work. Together
they cover the whole job: proving an assembled 6502 program still matches the
ROM it came from, browsing and scripting against its address-to-source
mapping, figuring out what its own symbol table and data accesses mean,
catching symbolization gaps, and exporting for other tools. In order, this
guide covers:

1. `--compare` — byte-for-byte parity against a reference binary
2. `--xref-summary` — a top-N orientation view of an unfamiliar binary
3. `--xref` with `--xref-data`/`--xref-include-owner` — exact, attributed data edges
4. Local and anonymous labels — the `--include-locals`/`--include-anon` scope filter
5. `--analyze-index-patterns` — how a table is actually walked
6. `--data-consumers` — per-table aggregate consumer view
7. `--analyze-data-coverage` — coverage only, no site lists
8. `--audit-raw-addresses` — raw literals that should be symbols
9. `--xref-instructions`/`--instruction-records-output` — the raw instruction stream
10. FCEUX debugger symbol export

This document walks through every one of those features against one small,
self-contained example program, using real `xasm` output (not hand-written
samples). For each feature it answers: what reverse-engineering question does
this let you answer, and what does the output actually look like when you ask
it.

**What this document is not:**

- It is not the schema reference. Field-by-field contracts, defaults, exit
  codes, and edge-case rules live in `XASM_REVERSE_ENGINEERING_FEATURES_SPEC.md`,
  `XASM_DATA_DIRECTIVE_REFERENCES_SPEC.md`, and
  `XASM_INDEX_BOUND_ANALYSIS_SPEC.md`. Read those when you need to know exactly
  what a field means in a case this guide doesn't cover.
- It is not a project workflow guide. It doesn't wrap these flags in Makefiles,
  wire them into a pass/ledger process, or assume any consuming project's
  directory layout. `xasm` is a general-purpose assembler; these features are
  useful on any 6502 project, and any project that builds a workflow around
  them should document that workflow itself.

## The example program

Every command below runs against this one file. It's deliberately small, but
it exercises a real mix of patterns: a shared subroutine called from two
places (plus once *not* through its symbol), a table walked with a
provably-bounded loop, two adjacent bytes read as a pair, a low/high pointer
table selected with a scaled index and then dereferenced indirectly, and one
small routine (`FindFreeSlot`) whose loop and branch target are deliberately
*not* named globally.

Save this as `re_example.asm` (source lines below are cited by this exact
numbering — no header comment, no leading blank line):

```asm
        DATASEG ZEROPAGE
        ORG $10
ZP_PTR_LO:
        DSB 1
ZP_PTR_HI:
        DSB 1

        CODESEG
        ORG $C000

Start:
        JSR InitPointers
        JSR ProcessEntities
        RTS

; A shared subroutine, called from two different places below.
ApplyVelocity:
        LDX #$00
ApplyVelocityLoop:
        LDA VelocityTable,X
        CLC
        ADC PositionTable,X
        STA PositionTable,X
        INX
        CPX #$04
        BNE ApplyVelocityLoop
        RTS

InitPointers:
        LDA #<VelocityTable
        STA ZP_PTR_LO
        LDA #>VelocityTable
        STA ZP_PTR_HI
        RTS

ProcessEntities:
        JSR ApplyVelocity

        ; Two adjacent reads one byte apart -> paired_byte_reads
        LDY #$01
        LDA FrequencyTable-1,Y
        STA $20
        LDA FrequencyTable,Y
        STA $21

        ; ASL A / TAX / LDA Table,X -> scaled_index_stride_2, feeding a
        ; low/high pointer pair that is then dereferenced indirectly.
        LDX #$00
        ASL A
        TAX
        LDA CommandLoTable,X
        STA ZP_PTR_LO
        LDA CommandHiTable,X
        STA ZP_PTR_HI
        LDY #$00
        LDA [ZP_PTR_LO],Y

        JSR ApplyVelocity

        ; A raw JSR to ApplyVelocity's address instead of using its label.
        JSR $C007
        RTS

VelocityTable:
        DB 1,2,3,4

PositionTable:
        DB 10,20,30,40

; Declared as 6 bytes; only offsets 0 and 1 are ever read directly above.
FrequencyTable:
        DB 5,7,9,11,13,15

CommandLoTable:
        DB <Cmd0,<Cmd1
CommandHiTable:
        DB >Cmd0,>Cmd1

Cmd0:
        LDA #$00
        RTS
Cmd1:
        LDA #$01
        RTS

; Scan for the first inactive entity slot. The loop continuation point and
; the "found it" branch target are private to this routine -> local (@@) and
; anonymous (+) labels, not globals.
FindFreeSlot:
        LDX #$00
@@scan:
        LDA EntityFlags,X
        BEQ +
        INX
        CPX #$04
        BNE @@scan
        LDX #$FF
+
        RTS

EntityFlags:
        DB 1,1,0,1

        END
```

It assembles cleanly:

```sh
xasm --pure-binary re_example.asm -o re_example.o
```

(`Start` and `FindFreeSlot` each trigger a "defined but not used" warning,
because nothing in this tiny file calls into them from outside — expected for
an entry point and a small self-contained routine in an isolated example, and
unrelated to the features below.)

---

## 1. `--compare`: does this still assemble to the exact same bytes?

**Why you'd reach for it:** everything else in this guide analyzes what your
*source* says. None of it is worth anything if that source doesn't actually
reproduce the ROM you're reverse-engineering. `--compare` is the feature this
whole exercise runs on: reassemble after every edit and prove, byte for byte,
that nothing changed except what you meant to change. It's the reason a
rename, a re-indentation, or a "purely cosmetic" refactor can be trusted at
all.

First, establish a known-good reference — here, simply the current build:

```sh
xasm --pure-binary re_example.asm -o re_example.o
cp re_example.o reference.o
```

Compare against it after any edit:

```sh
xasm --pure-binary re_example.asm -o re_example.o --compare=reference.o
```

When it matches, `--compare-format=text` (the default) prints nothing at all
for the comparison itself and exits `0` — silence is success. Ask for JSON and
you get an explicit report either way:

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --compare=reference.o --compare-format=json
```

```json
{
  "version": "1",
  "reference_file": "reference.o",
  "assembled_file": "re_example.o",
  "compared_length": 117,
  "match": true,
  "mismatches": [  ]
}
```

**Now make an edit that actually changes a byte.** Change `Cmd1`'s `LDA #$01`
to `LDA #$02` and reassemble against the *original* `reference.o`:

```sh
xasm --pure-binary re_example.asm -o re_example.o --compare=reference.o
```

```text
mismatch #1 at output+0x0060 (CPU $C060): expected 01 got 02
source: re_example.asm:83:9          LDA #$02
```

Exit code `5`. This is the entire point of the feature: it doesn't just say
"something's different," it hands you the exact byte offset, the CPU address,
*and* the source line responsible — no manual bisection of a hex dump required.
The JSON form (`--compare-format=json`) carries the identical fields
(`output_offset`, `cpu_address`, `expected_hex`/`actual_hex`, `source`) for
scripting a pass/fail gate instead of reading text.

**`--compare-max-mismatches` — the default hides how bad it is.** The default
is `1`, and the tool stops scanning at the first mismatch it finds — it does
not keep counting to tell you how many more there are. Introduce a *second*
wrong byte (`Cmd0`'s `LDA #$00` → `LDA #$03`, alongside the `Cmd1` edit above)
and compare with the default:

```text
mismatch #1 at output+0x005D (CPU $C05D): expected 00 got 03
source: re_example.asm:80:9          LDA #$03
```

Nothing tells you a second mismatch exists. Raise the limit and both show up:

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --compare=reference.o --compare-max-mismatches=5
```

```text
mismatch #1 at output+0x005D (CPU $C05D): expected 00 got 03
source: re_example.asm:80:9          LDA #$03
mismatch #2 at output+0x0060 (CPU $C060): expected 01 got 02
source: re_example.asm:83:9          LDA #$02
```

If a parity check is ever going to gate more than a single-byte fix, pass a
generous `--compare-max-mismatches` up front rather than fixing one byte,
rerunning, finding the next one, and repeating.

**`--compare-offset`/`--compare-length` — narrow the window, not just skip a
header.** Both apply to the *same* byte range in both files simultaneously,
so they're for comparing only a specific region of two otherwise-aligned
files — not for reconciling a reference and an assembled output whose leading
regions are different sizes. A real use: gate on the part of the ROM you've
actually finished verifying while the rest is still in progress. `VelocityTable`
starts at output offset `0x4A` (74) in this file, so comparing only the code
before it — offset `0`, length `74` — passes cleanly even with the two data
mismatches introduced above, because they fall outside that window:

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --compare=reference.o --compare-offset=0 --compare-length=74
```

Exits `0` — silent success — despite the full comparison above showing two
real mismatches further into the file.

**`--compare-cpu-base` you likely won't need here.** It supplies a fallback
CPU address for a mismatch when the assembler can't resolve one from the
source map. In practice that gap doesn't open up for reverse-engineering
source: `ORG` (and therefore any fixed-address 6502 disassembly) only
assembles in `--pure-binary` mode, and `--pure-binary` mismatches already
resolve a CPU address and source line automatically, as shown above — every
mismatch in this guide got one for free. It exists for invocation shapes
outside that path, not as something a `--pure-binary` parity check is missing.

**`--listing`/`--listing-format` — the general-purpose version of the same
mapping.** `--compare` only source-maps the bytes that actually mismatch. For
browsing or scripting against the *whole* address-to-source map — not just at
a known-bad offset — `--listing` is the tool, and JSON is the form worth using
(`--listing-format=text|json|ndjson`; text is the default and meant for eyes,
not scripts):

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --listing=re_example.listing.json --listing-format=json
```

One row, picked out by address — the same `JSR $C007` site from every section
below:

```json
{
  "line": 61, "column": 9, "source_text": "        JSR $C007",
  "cpu_address_start": "0xC046", "cpu_address_end": "0xC048",
  "output_offset_start": 70, "output_offset_end": 72,
  "bytes_hex": ["20", "07", "C0"],
  "directive_or_opcode": "JSR", "addressing_mode": "absolute"
}
```

This is what `make project-compare`-style parity-drift diagnosis in any
consuming project is built on: take a differing ROM offset, `jq`-filter the
JSON listing for the matching `output_offset_start`/`cpu_address_start`, and
you're at the exact source line and emitted bytes without a manual bisection
of the binary. It's also how `--compare`'s own mismatch reports get their
`source`/`cpu_address` fields — same lookup, just run for you automatically at
the one address that failed.

---

## 2. `--xref-summary`: orient yourself in an unfamiliar binary

**Why you'd reach for it:** the first question when facing an unfamiliar
disassembly is almost never "give me every reference" — it's "what are the
handful of things that matter here?" Which routines does everything call?
Which data tables does everything read? `--xref-summary` answers that without
requiring you to build or read a full cross-reference first.

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --xref-summary --xref-summary-format=json \
  --xref-summary-output=re_example.xref_summary.json
```

```json
{
  "top_callables": [
    {"label":"ApplyVelocity","addr":"0xC007","jsr_count":2,"jmp_count":0,"total_ref_count":2,
     "top_referring_routines":[{"routine":"ProcessEntities","count":2}],
     "first_run_terminator":"fallthrough","next_symbol_distance_bytes":2,
     "has_refs_from_other_routines":true,
     "nearby_symbols":["ApplyVelocityLoop","Start","InitPointers","ProcessEntities", "..."]},
    {"label":"InitPointers","addr":"0xC019","jsr_count":1, "..."},
    {"label":"ProcessEntities","addr":"0xC022","jsr_count":1, "..."}
  ],
  "top_jump_targets": [
    {"label":"ApplyVelocityLoop","addr":"0xC009","branch_count":1,"total_ref_count":1, "..."}
  ],
  "top_data_labels": [
    {"label":"ZP_PTR_LO","addr":"0x0010","read_count":1,"write_count":2,"total_ref_count":3, "..."},
    {"label":"ZP_PTR_HI","addr":"0x0011","read_count":0,"write_count":2,"total_ref_count":2, "..."},
    {"label":"PositionTable","addr":"0xC04E","read_count":1,"write_count":1, "..."},
    {"label":"FrequencyTable","addr":"0xC052","read_count":2,"write_count":0, "..."},
    {"label":"VelocityTable","addr":"0xC04A","read_count":1,"write_count":0, "..."},
    {"label":"CommandLoTable","addr":"0xC058","read_count":1,"write_count":0, "..."},
    {"label":"CommandHiTable","addr":"0xC05A","read_count":1,"write_count":0, "..."},
    {"label":"EntityFlags","addr":"0xC071","read_count":1,"write_count":0,"total_ref_count":1,
     "top_referring_routines":[{"routine":"FindFreeSlot","count":1}], "..."}
  ]
}
```

**What to notice:**

- `ApplyVelocity` surfaces immediately as the busiest callable, with its
  callers named in `top_referring_routines` — that's your starting point for
  understanding the program, found without reading a line of source.
- `ApplyVelocity.jsr_count` is **2**, not 3. There really are three places in
  this file that transfer control to `$C007`, but the summary is built from
  *symbolic* references — `JSR ApplyVelocity` — and one of the three call
  sites is a raw `JSR $C007`. A count-based summary like this one is blind to
  that call. Keep that in mind; it comes back in the `--audit-raw-addresses`
  section below.
- `ZP_PTR_LO`/`ZP_PTR_HI` show up as ordinary data labels because they *are*
  ordinary symbols — they're declared with `DSB 1` in a zero-page data
  segment rather than conjured out of raw `$10`/`$11` literals. That
  distinction — a named byte vs. a bare hex address — is what makes every
  other feature below able to talk about them at all.
- `first_run_terminator: "fallthrough"` on `ApplyVelocity` is worth reading
  literally: decoding from `ApplyVelocity` reaches the next non-local symbol
  (`ApplyVelocityLoop`) without hitting `RTS`/`RTI`/`JMP` first, because
  `ApplyVelocityLoop` is the loop body inside the same routine. It is not a
  claim that the routine itself falls through into unrelated code.
- `EntityFlags` shows up as an ordinary data label, but `FindFreeSlot`'s own
  loop label and branch target don't appear anywhere above — not as a jump
  target, not in anyone's `nearby_symbols`. They're a local label and an
  anonymous label, and every feature in this guide hides those by default.
  Section 4 covers why, and how to turn that off.

**Narrowing the summary with `--xref-summary-include`/`--xref-summary-exclude`.**
On a large disassembly, `top_data_labels` can run long; both take a regex
matched against the label name. `--xref-summary-limit`,
`--xref-summary-top-referrers`, and `--xref-summary-nearby-window` (defaults
25, 5, and 128) tune the size of each section the same way. Restrict the data
section to names containing `Table`:

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --xref-summary --xref-summary-format=json --xref-summary-kind=data \
  --xref-summary-include='Table'
```

This drops `ZP_PTR_LO`, `ZP_PTR_HI`, and `EntityFlags` from `top_data_labels`,
leaving only `PositionTable`, `FrequencyTable`, `VelocityTable`,
`CommandLoTable`, and `CommandHiTable` — the same five records, just filtered
by name rather than by kind.

---

## 3. `--xref` with `--xref-data=true --xref-include-owner=true`: exact, attributed data edges

**Why you'd reach for it:** once you know *what* matters, you need proof —
every exact site that reads or writes a given byte, and which routine is
responsible for it. This is the tool for "does anything besides `ApplyVelocity`
touch `PositionTable`?" or "if I resize `FrequencyTable`, what breaks?"

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --xref=re_example.xref.json --xref-format=json \
  --xref-data=true --xref-include-owner=true
```

`data_reads` / `data_writes` (owner-attributed, one record per site):

```json
{
  "symbol": "FrequencyTable", "site_addr": "0xC027",
  "owner_routine": "ProcessEntities", "owner_routine_addr": "0xc022",
  "displacement": -1, "addressing_mode": "absolute_y"
}
```

`owner_routine` is the nearest preceding global code label — a lexical fact,
not a claim about the call graph — but it's exactly the fact you want when
triaging "which routine's rename sweep touches this."

**Indirect pointer-chase tracking.** The example builds a pointer in
`ZP_PTR_LO`/`ZP_PTR_HI` from `CommandLoTable`/`CommandHiTable` and then reads
through it with `LDA [ZP_PTR_LO],Y`. `--xref-data=true` follows that:

```json
"indirect_data_flows": [
  {
    "ptr_symbol": "ZP_PTR_LO",
    "producer_site": "0xC03D",
    "consumer_site": "0xC041",
    "access_kind": "read",
    "owner_routine": "ProcessEntities",
    "owner_routine_addr": "0xc022"
  }
]
```

This is deliberately conservative: it only fires when both bytes of the
pointer are known symbols (this is *why* the example declares `ZP_PTR_LO`/
`ZP_PTR_HI` as real labels instead of using bare `$10`/`$11`) and both are
written, in the same routine, with nothing invalidating them in between the
write and the dereference. If you point `xasm` at zero-page scratch bytes that
have no names, this array comes back empty — that's not a bug, it's the
feature declining to invent a name for something it can't identify.

**Data-directive references.** The same `--xref-data=true` invocation also
reconstructs what each byte of a `.DB`/`.DW` table actually points at — this
is how `CommandLoTable`/`CommandHiTable` get proven to be a low/high split
pointer table without anyone eyeballing `<`/`>` operators:

```json
"data_directive_references": [
  {
    "expression": "<Cmd0", "referenced_symbols": ["Cmd0"],
    "use_cpu_address": "0xC058", "owner_symbol": "CommandLoTable",
    "owner_item_index": 0,
    "target_symbol": "Cmd0", "target_projection": "low", "target_kind": "code"
  }
]
```

`target_kind: "code"` and `target_projection: "low"` together are the whole
story: byte 0 of `CommandLoTable` is the low byte of a pointer to a routine.
Four of these records (two tables × two entries) fully describe the table
pair — that's a complete, provable reconstruction of a jump/dispatch table
from the assembler's own evaluation of the expressions, not from re-parsing
source text.

---

## 4. Local and anonymous labels: what's hidden by default, and why

**Why you'd reach for the include flags:** `FindFreeSlot`'s loop continuation
point and its "found a slot" branch target don't need names anyone else will
ever reference — they're private to that one routine. Every feature in this
guide hides labels like that by default, precisely so a summary of a
50-routine file isn't 90% loop-continuation and skip-branch noise. But
sometimes the noise *is* the point — e.g. proving that literally every branch
target in a routine is accounted for, not just the ones that happened to get
promoted to a real name. That's what `--include-locals`/`--include-anon` are
for (also spelled `--xref-include-locals`/`--xref-include-anon` — same
setting, more on that below).

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --xref-summary --xref-summary-format=json --xref-summary-kind=jump_target \
  --xref-summary-output=re_example.jump_targets.json
```

```json
{
  "top_jump_targets": [
    {"label":"ApplyVelocityLoop","addr":"0xC009","branch_count":1,"total_ref_count":1, "..."}
  ]
}
```

Same command, `--include-locals=true --include-anon=true` added:

```json
{
  "top_jump_targets": [
    {"label":"ApplyVelocityLoop","addr":"0xC009","branch_count":1,"total_ref_count":1, "..."},
    {"label":"@@scan#15","addr":"0xC064","branch_count":1,"total_ref_count":1,
     "top_referring_routines":[{"routine":"FindFreeSlot","count":1}], "..."},
    {"label":"+#0#0","addr":"0xC070","branch_count":1,"total_ref_count":1,
     "top_referring_routines":[{"routine":"FindFreeSlot","count":1}], "..."}
  ]
}
```

**What to notice:**

- `FindFreeSlot`'s loop label and branch target were there the whole time —
  they were just filtered, not missing. `top_referring_routines` correctly
  attributes both to `FindFreeSlot`, never to a local/anonymous label itself:
  a local label can never be a routine owner (the same ownership rule from
  Section 3).
- The names change shape: `@@scan` becomes `@@scan#15`, and the anonymous `+`
  becomes `+#0#0`. Neither `@@scan` nor `+` is unique on its own — every
  routine in a real program can have its own `@@scan`, and `+` can be reused
  any number of times — so xasm appends a disambiguating suffix once you ask
  it to surface them. Don't expect to see a bare `@@scan` or `+` in any
  feature's output.
- **`--include-locals`/`--include-anon` and `--xref-include-locals`/
  `--xref-include-anon` are two spellings of the same setting, not two
  independent ones.** There is one locals switch and one anonymous switch per
  invocation. Either spelling sets it:

  ```sh
  xasm --pure-binary re_example.asm -o re_example.o \
    --xref=re_example.xref.json --xref-format=json \
    --xref-include-locals=true --xref-include-anon=true
  ```

  is equivalent to the same command with `--include-locals=true
  --include-anon=true` instead. Without either spelling, `@@scan`/`+` are
  simply absent from the `symbols` and `references` arrays in Section 3's
  `--xref` output above. If a command line somehow passes both spellings with
  different values, whichever comes last wins — but there's no reason to pass
  both. One consequence worth knowing: within a single invocation you cannot
  ask for locals in `--xref` output but not in `--xref-summary` — it's a
  per-run setting, not a per-output one.
- **This setting reaches `--xref`, `--xref-summary`, and
  `--analyze-index-patterns` — not `--data-consumers` or
  `--analyze-data-coverage`.** The latter two aggregate by *declared data
  span* (Section 6), and a local or anonymous label can never anchor a span in
  the first place — the rule requiring a non-local symbol is unconditional,
  with no flag to override it. Passing `--include-locals=true` to
  `--data-consumers` doesn't error, it just has nothing to do: a local data
  table is invisible to it regardless.

---

## 5. `--analyze-index-patterns`: how is this table actually walked?

**Why you'd reach for it:** a table's *declaration* (`.DB` bytes) tells you
its size. It tells you nothing about its *shape* — is it a flat byte array
indexed 0..N? A table of 16-bit words accessed as adjacent byte pairs? Two
parallel tables selected by a doubled index? This feature reads that shape
back out of the instructions that touch the table.

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --analyze-index-patterns --index-patterns-format=json \
  --index-patterns-output=re_example.index_patterns.json
```

```json
[
  {"table_label":"VelocityTable","routine":"ApplyVelocityLoop","site_addr":"0xC009",
   "access_kind":"read","access_pattern":"base","index_register":"X","displacement":0,
   "index_value_source_kind":"register","index_upper_bound":4,"index_bound_kind":"compare"},

  {"table_label":"PositionTable","routine":"ApplyVelocityLoop","site_addr":"0xC00D",
   "access_kind":"read","access_pattern":"base","index_register":"X","displacement":0,
   "index_value_source_kind":"register","index_upper_bound":4,"index_bound_kind":"compare"},

  {"table_label":"PositionTable","routine":"ApplyVelocityLoop","site_addr":"0xC010",
   "access_kind":"write","access_pattern":"base","index_register":"X","displacement":0,
   "index_value_source_kind":"register","index_upper_bound":4,"index_bound_kind":"compare",
   "evidence_flags":["write_access"]},

  {"table_label":"FrequencyTable","routine":"ProcessEntities","site_addr":"0xC027",
   "access_kind":"read","access_pattern":"paired_byte_reads","index_register":"Y",
   "displacement":-1,"index_value_source_kind":"immediate","estimated_record_width":2,
   "evidence_flags":["negative_displacement","adjacent_read_pair"]},

  {"table_label":"FrequencyTable","routine":"ProcessEntities","site_addr":"0xC02C",
   "access_kind":"read","access_pattern":"base","index_register":"Y","displacement":0,
   "index_value_source_kind":"immediate"},

  {"table_label":"CommandLoTable","routine":"ProcessEntities","site_addr":"0xC035",
   "access_kind":"read","access_pattern":"scaled_index_stride_2","index_register":"X",
   "displacement":0,"index_value_source_kind":"scaled_accumulator",
   "estimated_record_width":2,"evidence_flags":["scaled_index"]},

  {"table_label":"CommandHiTable","routine":"ProcessEntities","site_addr":"0xC03A",
   "access_kind":"read","access_pattern":"scaled_index_stride_2","index_register":"X",
   "displacement":0,"index_value_source_kind":"scaled_accumulator",
   "estimated_record_width":2,"evidence_flags":["scaled_index"]},

  {"table_label":"EntityFlags","routine":"FindFreeSlot","site_addr":"0xC064",
   "access_kind":"read","access_pattern":"base","index_register":"X","displacement":0,
   "index_value_source_kind":"register","index_upper_bound":4,"index_bound_kind":"compare"}
]
```

(This is the complete output for this file — every indexed access site gets exactly one
record.)

**What to notice, one pattern at a time:**

- `VelocityTable`/`PositionTable` come back as plain `base` accesses — but
  with `index_upper_bound: 4, index_bound_kind: "compare"` attached. That's
  not guessed: it's read straight off this file's `CPX #$04` / `BNE` loop
  test. It proves, mechanically, that `X` never exceeds 3 here — exactly the
  fact you'd want before trusting that a 4-byte table declaration is complete
  and nothing walks past it. (Full rules for this in
  `XASM_INDEX_BOUND_ANALYSIS_SPEC.md`.)
- `FrequencyTable` is flagged `paired_byte_reads`: two reads one byte apart in
  the same local window (`FrequencyTable-1,Y` then `FrequencyTable,Y`). That's
  the fingerprint of a 16-bit value being reconstructed from two adjacent
  bytes — worth a second look even though each read individually looks like
  an ordinary indexed byte access.
- `CommandLoTable`/`CommandHiTable` come back `scaled_index_stride_2` with
  `index_value_source_kind: "scaled_accumulator"` — the `ASL A` / `TAX` before
  the read is exactly the "double the index to walk a table of 16-bit
  entries" idiom, here used to select one column of a low/high pointer pair.
  Combined with the `indirect_data_flows` record above, this is a complete,
  mechanically-derived description of a dispatch table: selected by a doubled
  index, split across two byte tables, dereferenced indirectly.
- `EntityFlags`'s record attributes to `routine: "FindFreeSlot"`, even though
  the read instruction is lexically inside `@@scan`. Local labels never own
  records — see Section 4.

**Why `CommandLoTable`/`CommandHiTable` didn't come back as `split_lo_hi_tables`.**
That pattern exists specifically for this shape and takes precedence over
every other pattern when it matches — but matching is suffix-based, and `Lo`/
`Hi` sit in the *middle* of these names (`Table` follows them), not at the
end. A separate file, `splitpair.asm`, with a genuine suffix pair — recognized
automatically, no flag needed:

```asm
        ORG $C000
ReadPointer:
        LDX #$00
        LDA PointerTableLo,X
        STA $10
        LDA PointerTableHi,X
        STA $11
        RTS
PointerTableLo:
        DB <Cmd0,<Cmd1
PointerTableHi:
        DB >Cmd0,>Cmd1
Cmd0:
        RTS
Cmd1:
        RTS
        END
```

```sh
xasm --pure-binary splitpair.asm -o splitpair.o \
  --analyze-index-patterns --index-patterns-format=json
```

```json
[
  {"table_label":"PointerTable","routine":"ReadPointer","site_addr":"0xC002","access_kind":"read","access_pattern":"split_lo_hi_tables","index_register":"X","displacement":0,"index_value_source_kind":"immediate","table_label_lo":"PointerTableLo","table_label_hi":"PointerTableHi","evidence_flags":["split_named_lo_hi"]},
  {"table_label":"PointerTableHi","routine":"ReadPointer","site_addr":"0xC007","access_kind":"read","access_pattern":"base","index_register":"X","displacement":0,"index_value_source_kind":"immediate"}
]
```

The pair collapses into one `split_lo_hi_tables` record keyed by the shared
stem (`PointerTable`); the second read still also gets its own ordinary `base`
record, so both sites remain individually visible.

The default suffix pairs are `Lo:Hi`, `Low:High`, `_lo:_hi`, `_low:_high`
(case-sensitive). For any other naming convention,
`--index-patterns-split-pairs` adds your own. Rename the same two tables to
`PointerTableA`/`PointerTableB` (an unrecognized suffix) and, by default, each
gets its own independent `base` record — no merge:

```json
[
  {"table_label":"PointerTableA","routine":"ReadPointer","site_addr":"0xC002","access_kind":"read","access_pattern":"base","index_register":"X","displacement":0,"index_value_source_kind":"immediate"},
  {"table_label":"PointerTableB","routine":"ReadPointer","site_addr":"0xC007","access_kind":"read","access_pattern":"base","index_register":"X","displacement":0,"index_value_source_kind":"immediate"}
]
```

Add `--index-patterns-split-pairs='A:B'` and the merge reappears, keyed by the
custom suffix pair instead of a default one:

```sh
xasm --pure-binary splitpair.asm -o splitpair.o \
  --analyze-index-patterns --index-patterns-format=json \
  --index-patterns-split-pairs='A:B'
```

```json
[
  {"table_label":"PointerTable","routine":"ReadPointer","site_addr":"0xC002","access_kind":"read","access_pattern":"split_lo_hi_tables","index_register":"X","displacement":0,"index_value_source_kind":"immediate","table_label_lo":"PointerTableA","table_label_hi":"PointerTableB","evidence_flags":["split_named_lo_hi"]},
  {"table_label":"PointerTableB","routine":"ReadPointer","site_addr":"0xC007","access_kind":"read","access_pattern":"base","index_register":"X","displacement":0,"index_value_source_kind":"immediate"}
]
```

---

## 6. `--data-consumers`: aggregate view, one record per table

**Why you'd reach for it:** `--analyze-index-patterns` and `--xref-data` both
report *per-site* facts. When the question is about the *table* — how many
routines touch it, which displacements are observed, how much of its declared
span is actually read or written — you want them pre-aggregated instead of
folding site lists yourself.

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --data-consumers --data-consumers-format=json \
  --data-consumers-output=re_example.data_consumers.json
```

```json
{
  "label": "ZP_PTR_LO", "declared_start": "0x0010", "declared_end_exclusive": "0x0011",
  "declared_size": 1, "read_site_count": 0, "write_site_count": 2,
  "distinct_routine_count": 2, "observed_constant_displacements": [0],
  "covered_ranges": [{"start":"0x0010","end_exclusive":"0x0011"}],
  "uncovered_ranges": [], "has_indexed_accesses_without_exact_coverage": false,
  "access_patterns": ["base"],
  "write_sites": [
    {"routine":"InitPointers","site_addr":"0xC01B","displacement":0,"addressing_mode":"absolute"},
    {"routine":"ProcessEntities","site_addr":"0xC038","displacement":0,"addressing_mode":"absolute"}
  ]
}
```

`distinct_routine_count: 2` is the whole point of this record: `ZP_PTR_LO` is
a shared scratch pointer written by two unrelated-looking routines
(`InitPointers` and `ProcessEntities`). That's exactly the kind of fact that's
easy to miss reading source top-to-bottom and immediate to see aggregated
here — and it's a strong signal *not* to treat this byte as private state of
either routine.

**`--include-overlaps` — when two names point at the same bytes.** Nothing in
this file has an overlap, so here's a minimal one: two labels declared back to
back, both anchoring the same table. A separate file, `overlap.asm`:

```asm
        ORG $C000
Reader:
        LDX #$00
        LDA PrimaryTable,X
        LDA PrimaryTableAlias,X
        RTS
PrimaryTable:
PrimaryTableAlias:
        DB 1,2,3,4,5,6
        END
```

By definition (Section 3.2 of `XASM_REVERSE_ENGINEERING_FEATURES_SPEC.md`),
the first non-local symbol at an address owns the primary span; a later symbol
at that same address is an *overlap symbol*, excluded by default:

```sh
xasm --pure-binary overlap.asm -o overlap.o \
  --data-consumers --data-consumers-format=json
```

```json
[
  {"label":"PrimaryTable","declared_start":"0xC009","declared_end_exclusive":"0xC00F",
   "declared_size":6,"read_site_count":2,"write_site_count":0,"distinct_routine_count":1,
   "read_sites":[
     {"routine":"Reader","site_addr":"0xC002","displacement":0,"addressing_mode":"absolute_x"},
     {"routine":"Reader","site_addr":"0xC005","displacement":0,"addressing_mode":"absolute_x"}]}
]
```

`read_site_count: 2` already tells you the table is read twice — by default,
a read through the alias is silently folded into the canonical name's record
rather than dropped, so the top-level count stays honest even though you
haven't seen `PrimaryTableAlias` mentioned anywhere. Add `--include-overlaps=true`
and each name gets its own separate record instead, split by which name was
actually used at each site:

```sh
xasm --pure-binary overlap.asm -o overlap.o \
  --data-consumers --data-consumers-format=json --include-overlaps=true
```

```json
[
  {"label":"PrimaryTable","declared_start":"0xC009","declared_end_exclusive":"0xC00F",
   "declared_size":6,"read_site_count":1,"write_site_count":0,"distinct_routine_count":1,
   "read_sites":[{"routine":"Reader","site_addr":"0xC002","displacement":0,"addressing_mode":"absolute_x"}]},
  {"label":"PrimaryTableAlias","declared_start":"0xC009","declared_end_exclusive":"0xC00F",
   "declared_size":6,"read_site_count":1,"write_site_count":0,"distinct_routine_count":1,
   "read_sites":[{"routine":"Reader","site_addr":"0xC005","displacement":0,"addressing_mode":"absolute_x"}]}
]
```

Reach for this when a rename sweep needs to know whether an alias is still
used anywhere before it's deleted — the default view can't tell you that, by
design.

---

## 7. `--analyze-data-coverage`: just the coverage question

**Why you'd reach for it:** `--data-consumers` gives you everything; if all
you actually want is "does this declared span have bytes nothing reads or
writes," the narrower `--analyze-data-coverage` is cheaper to generate and
easier to scan across a large table set.

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --analyze-data-coverage --data-coverage-format=json \
  --data-coverage-output=re_example.data_coverage.json
```

Two records from the array — the full output has one per eligible data label
(`ZP_PTR_LO`, `ZP_PTR_HI`, `VelocityTable`, `PositionTable`, `FrequencyTable`,
`CommandLoTable`, `CommandHiTable`, `EntityFlags`):

```json
[
  {"label":"VelocityTable","declared_size":4,"covered_ranges":[],"covered_size":0,
   "uncovered_ranges":[{"start":"0xC04A","end_exclusive":"0xC04E"}],"uncovered_size":4,
   "access_count":1,"has_indexed_accesses_without_exact_coverage":true},

  {"label":"FrequencyTable","declared_size":6,
   "covered_ranges":[{"start":"0xC052","end_exclusive":"0xC054"}],"covered_size":2,
   "uncovered_ranges":[{"start":"0xC054","end_exclusive":"0xC058"}],"uncovered_size":4,
   "access_count":2,"has_indexed_accesses_without_exact_coverage":false}
]
```

**Read these two side by side — they look similar and mean opposite things:**

- `VelocityTable` shows `uncovered_size: 4` (its *entire* declared span) *and*
  `has_indexed_accesses_without_exact_coverage: true`. That flag is the
  disclaimer: the only access is an indexed read (`VelocityTable,X`) whose
  runtime offset isn't a compile-time constant, so exact per-byte coverage
  genuinely can't be computed here — this is a **known-indexed, not
  provably-empty** table. (Section 5 already proved the loop touches all 4
  bytes via `index_upper_bound`; this feature just doesn't have that context.)
- `FrequencyTable` shows `uncovered_size: 4` too, but
  `has_indexed_accesses_without_exact_coverage: false`. Every access here was
  exact (constant-displacement `paired_byte_reads`), so this *is* a real,
  provable finding: two of this table's six declared bytes are read, and four
  never are, by anything this assembly can see. That's a genuine candidate for
  "this table was declared bigger than it's used, or something reads it
  another way you haven't found yet."

The flag is what separates "we don't know" from "we checked, and no."

`--include-overlaps` works identically here — see Section 6 — splitting a
merged access count back out per alias name instead of leaving it folded into
the canonical symbol's record.

---

## 8. `--audit-raw-addresses`: symbols hiding in plain (hex) sight

**Why you'd reach for it:** every feature above builds its picture from
*symbolic* references — `JSR ApplyVelocity`, `LDA Table,X`. A raw numeric
operand that happens to equal a label's address is invisible to all of them:
it assembles to the identical byte, executes identically, and simply doesn't
appear as a reference to anything. Section 2 already surfaced the symptom
(`ApplyVelocity.jsr_count` under-counting by one); this feature finds the
cause.

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --audit-raw-addresses --audit-rom-range='$C000-$FFFF' --audit-output-format=text
```

```text
re_example.asm:11: warning: `Start' defined but not used
re_example.asm:89: warning: `FindFreeSlot' defined but not used
re_example.asm:61:9: warning A100: raw control-flow target '$C007' can use symbol 'ApplyVelocity'
```

The first two lines are the same unused-label warnings from the top of this
guide — unrelated to the audit, printed by the ordinary assembly pass that
runs before it. The third is the one that matters here: that's the third
`ApplyVelocity` call site, `JSR $C007`, caught and named
exactly. Fix it to `JSR ApplyVelocity` and every count in `--xref-summary` and
every owner-attributed edge in `--xref-data` becomes accurate — the analysis
features aren't wrong about this program, they were only ever as complete as
its symbolization. `--audit-raw-addresses` is what closes that gap; it also
catches raw pointer-split immediates (`A110`, e.g. `LDA #<$C007`) and raw
zero-page operands that match a known `EQU` (`A120`) using the same idea.

**Making it a hard gate.** By default (`--audit-level=warn`, the default
shown above) a finding is just a warning: it prints and the build still
succeeds — `exit=0`. Add `--audit-level=error` and the identical finding
comes back reclassified, exit code included:

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --audit-raw-addresses --audit-rom-range='$C000-$FFFF' \
  --audit-level=error --audit-output-format=text
```

```text
re_example.asm:11: warning: `Start' defined but not used
re_example.asm:89: warning: `FindFreeSlot' defined but not used
re_example.asm:61:9: error A100: raw control-flow target '$C007' can use symbol 'ApplyVelocity'
```

The line itself changes from `warning A100` to `error A100`, and the process
exits with status `4` instead of `0` — the two unused-label warnings above it
stay warnings and don't affect the exit code; only audit findings do. That's
the difference between "the audit noticed this" and "fail the build on this":
point a CI step at `--audit-level=error` once symbolization work is caught up,
and any new raw address regresses the build instead of scrolling past in a
warning log nobody reads.

**Raw pointer tables (`A130`, `A131`) and what `--audit-rom-range` actually
gates.** Beyond raw code addresses, the audit also catches raw pointer *data*
— a `.DW` word or a `.DB` low/high byte pair that reconstructs a known label's
address without naming it. A separate small file, `pointers.asm`:

```asm
        ORG $C000
Target:
        RTS
PointerWordTable:
        DW $C000
PointerByteTable:
        DB $00,$C0
        END
```

```sh
xasm --pure-binary pointers.asm -o pointers.o \
  --audit-raw-addresses --audit-rom-range='$C000-$FFFF' --audit-output-format=text
```

```text
pointers.asm:2: warning: `Target' defined but not used
pointers.asm:4: warning: `PointerWordTable' defined but not used
pointers.asm:6: warning: `PointerByteTable' defined but not used
pointers.asm:5:12: warning A130: raw .DW target '$C000' can use symbol 'Target'
pointers.asm:7:12: warning A131: raw .DB pointer pair '$00,$C0' can use symbol 'Target'
```

`--audit-rom-range=LO-HI` narrows candidate values to a range before checking
them against known labels — but, verified against the code, it gates **only
`A131`**, the `.DB` byte-pair heuristic. Any two adjacent bytes can coincidentally
form a 16-bit value that happens to match some label's address, so bounding
the search to the ROM range cuts that noise; `A130` (a `.DW` word), `A100`
(control flow), `A110`, and `A120` fire the same regardless of whether
`--audit-rom-range` is given at all. Re-running with
`--audit-rom-range='$D000-$FFFF'` — deliberately excluding `$C000` — makes
`A131` disappear while `A130` still fires on the identical address:

```text
pointers.asm:2: warning: `Target' defined but not used
pointers.asm:4: warning: `PointerWordTable' defined but not used
pointers.asm:6: warning: `PointerByteTable' defined but not used
pointers.asm:5:12: warning A130: raw .DW target '$C000' can use symbol 'Target'
```

Don't expect `--audit-rom-range` to scope the whole audit; it's a single
false-positive guard on one of the five finding types.

**Suppressing one finding without turning off the audit.** Sometimes a raw
address is deliberate — a known runtime-computed vector, a byte pattern that
only coincidentally matches a label — and re-litigating it on every run is
noise. Two inline-comment directives handle that without an extra flag. A
separate file, `audit_ignore.asm`:

```asm
        ORG $C000
Target:
        RTS
Start:
        JSR $C000 ; xasm:audit-ignore A100
; xasm:audit-disable
        LDA #<$C000
; xasm:audit-enable
        LDA #>$C000
        RTS
        END
```

```sh
xasm --pure-binary audit_ignore.asm -o audit_ignore.o \
  --audit-raw-addresses --audit-rom-range='$C000-$FFFF' --audit-output-format=text
```

```text
audit_ignore.asm:2: warning: `Target' defined but not used
audit_ignore.asm:4: warning: `Start' defined but not used
audit_ignore.asm:9:9: warning A110: raw pointer immediate split '#>$C000' can use symbol 'Target'
```

Only one finding survives, and it's instructive which one. `xasm:audit-ignore
A100` on the `JSR $C000` line suppresses exactly that finding, by code, on
that line only — a comma-separated list (`xasm:audit-ignore A100,A110`)
suppresses more than one. `xasm:audit-disable` / `xasm:audit-enable` bracket a
*region*: every finding between them is dropped regardless of code, which is
why line 7's `LDA #<$C000` never appears. Line 9's `LDA #>$C000` sits *after*
`xasm:audit-enable`, so the same kind of finding (`A110`) fires there
normally — proof the disabled block's boundary is exactly the two comments,
not "the rest of the routine." Prefer `xasm:audit-ignore` for a single
deliberate exception; reach for a disable/enable block only around a region
you've already reviewed as a whole.

---

## 9. `--xref-instructions`/`--instruction-records-output`: the raw instruction stream

**Why you'd reach for it:** every feature above is a pre-built answer to a
specific question. If you're writing your own analysis — a custom scanner, a
liveness check, anything that needs the complete, decoded instruction stream
rather than one of these curated views — this is the substrate: every emitted
instruction, independent of local/anonymous symbol filters, with its
pre-fold source expression, final bytes, and addressing mode.

Embedded in the ordinary `--xref` output:

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --xref=re_example.xref.json --xref-format=json --xref-instructions=true
```

The same `JSR $C007` site as everywhere else in this guide, now as a raw
instruction fact rather than an interpretation of one:

```json
{
  "origin_id": 34,
  "mnemonic": "JSR",
  "cpu_address": 49222,
  "addressing_mode": "absolute",
  "operand_form": "integer_literal",
  "referenced_symbols": [],
  "bytes": [32, 7, 192]
}
```

(`cpu_address` is decimal here — `49222` is `0xC046` — unlike the hex-string
`"0xC046"` used everywhere else in `--xref` output; this substrate is a
separate, independently-versioned contract, not a reformatting of the same
data.) `operand_form: "integer_literal"` and an empty `referenced_symbols` are
the same fact Sections 2 and 8 already surfaced two other ways — the summary
undercount and the `A100` finding — proven a third time, directly, with no
interpretation layer in between. That's the point of this substrate: build
whatever check you need on top of facts this exact, instead of re-deriving
them from source text or from a curated view that only answers the questions
it was designed to answer.

To get the same records as a standalone file instead of embedded in `--xref`,
use `--instruction-records-output=FILE`. That path requires
`--dependency-manifest=FILE` alongside it — a content-hash record of every
consumed input, so a later consumer can verify what actually produced the
artifact before trusting it:

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --instruction-records-output=re_example.instructions.json \
  --dependency-manifest=re_example.dependencies.json
```

Full field contract, versioning, and the manifest's schema:
`XASM_INSTRUCTION_RECORDS_SPEC.md` and `XASM_DEPENDENCY_MANIFEST_SPEC.md`.

---

## 10. FCEUX debugger symbol export

**Why you'd reach for it:** every feature so far analyzes source. This one
feeds a *different* tool — exporting every symbol as FCEUX `.nl` label files,
so the names in this guide show up in a live debugger session instead of bare
hex while you're single-stepping the real ROM.

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --fceux-nl-rom-prefix=re_example.nes. \
  --fceux-nl-ram-output=re_example.nes.ram.nl
```

`re_example.nes.0.nl` (bank 0 — addresses `>= $8000`, one file per 16 KiB PRG
page, hex bank number):

```text
$C000#Start#
$C007#ApplyVelocity#
$C009#ApplyVelocityLoop#
...
$C062#FindFreeSlot#
$C064#FindFreeSlot@@scan#
$C071#EntityFlags#
```

`re_example.nes.ram.nl` (addresses `< $8000`):

```text
$0010#ZP_PTR_LO#
$0011#ZP_PTR_HI#
```

Two things worth noticing against everything earlier in this guide: local
labels are named and exported here (`FindFreeSlot@@scan`, qualified by its
owning routine) with no `--include-locals`/`--xref-include-locals` needed —
README confirms this explicitly: NL export includes locals independently of
every xref scope flag. The anonymous `+` label, unsurprisingly, isn't
exported at all — there's no stable name to give it. For a 16 KiB NROM image
that's mapped into both `$8000-$BFFF` and `$C000-$FFFF`, add
`--fceux-nl-mirror-16k` to label both CPU windows from the one build (requires
exactly 16384 emitted bytes). Full behavior, including page-numbering and
sidecar file management: `XASM_FCEUX_NL_EXPORT_SPEC.md`.

---

## Combining features in one pass

Every output above accepts an explicit `-output=FILE`, and when every
requested feature has one, they can all run in a single `xasm` invocation:

```sh
xasm --pure-binary re_example.asm -o re_example.o \
  --xref=re_example.xref.json --xref-format=json \
  --xref-data=true --xref-include-owner=true \
  --xref-summary --xref-summary-output=re_example.xref_summary.json \
  --xref-summary-format=json \
  --analyze-index-patterns \
  --index-patterns-output=re_example.index_patterns.json \
  --index-patterns-format=json \
  --data-consumers \
  --data-consumers-output=re_example.data_consumers.json \
  --data-consumers-format=json \
  --analyze-data-coverage \
  --data-coverage-output=re_example.data_coverage.json \
  --data-coverage-format=json
```

This isn't just convenience: on a large, banked disassembly, the instruction
and symbol tables `xasm` builds to answer these questions are the expensive
part, and they're shared across every feature in one invocation. See
`XASM_XREF_PERFORMANCE_SPEC.md` for measurements — running these separately
per-feature costs roughly the same as `--xref-data=true` alone run *N* times.

## Where to go next

| Feature | Exact field/default/exit-code contract |
|---|---|
| `--compare` and its `-offset`/`-length`/`-max-mismatches`/`-format`/`-cpu-base` options | `README` ("Assembly and verification") |
| `--listing`/`--listing-format` | `README` ("Listings and cross-references") |
| `--xref-summary` (incl. `-include`/`-exclude`/`-limit`/`-top-referrers`/`-nearby-window`), `--xref-data`, `--xref-include-owner`, `--analyze-index-patterns` (incl. `-split-pairs`), `--data-consumers`, `--analyze-data-coverage`, `--include-locals`/`--include-anon`, `--include-overlaps` | `XASM_REVERSE_ENGINEERING_FEATURES_SPEC.md` |
| `data_directive_references` (part of `--xref-data=true`) | `XASM_DATA_DIRECTIVE_REFERENCES_SPEC.md` |
| `index_upper_bound` / `index_bound_kind` (part of `--analyze-index-patterns`) | `XASM_INDEX_BOUND_ANALYSIS_SPEC.md` |
| `--audit-raw-addresses`, `--audit-level`, `--audit-rom-range`, `--audit-output-format` (finding codes `A100`-`A131`) | `README` ("Analysis and diagnostics") and `xasm --help` |
| Structured per-instruction records (`--xref-instructions`, `--instruction-records-output`) | `XASM_INSTRUCTION_RECORDS_SPEC.md` |
| `--dependency-manifest` | `XASM_DEPENDENCY_MANIFEST_SPEC.md` |
| `--fceux-nl-rom-prefix`/`--fceux-nl-ram-output`/`--fceux-nl-mirror-16k` | `XASM_FCEUX_NL_EXPORT_SPEC.md` |
| Performance characteristics on large/banked inputs | `XASM_XREF_PERFORMANCE_SPEC.md` |

`xasm --help` is always the source of truth for exact flag names and defaults
on the build you have installed.
