# xasm Index-Bound Analysis Spec

## Status

Implemented and shipped: `--analyze-index-patterns` emits `index_upper_bound`
and `index_bound_kind` exactly as described below, for both the mask and
compare idioms, including symbolic-immediate resolution. `tests/regression.sh`
has a dedicated `run_expect_index_bounds` covering every scenario listed under
Testing, plus a register-mismatch and an unrelated-mask negative case. The
rest of this document is still phrased as a proposal ("Proposed Solution",
"Acceptance Criteria"); read those sections as a description of what shipped,
not a pending design.

## Purpose

Extend `--analyze-index-patterns` to emit the *resolved upper bound* of an
indexed table access whenever the assembler can prove one from a masking or
comparison idiom. Output schema and general index-pattern semantics remain
owned by `XASM_REVERSE_ENGINEERING_FEATURES_SPEC.md`; this spec adds two
fields and the analysis that populates them.

## Problem

The index-pattern analysis already resolves, per indexed access site, the
`index_register`, `index_value_source_kind`, `access_pattern`, and
`estimated_record_width`. It stops short of the one fact a size-guardrail
consumer needs most: **the proven bound on the index**.

A downstream tool (`nesrev`'s data-extent-assertion detector) wants to answer
"is table *T* read with a fixed, provable index bound of *N*?" so it can flag a
fixed-size table that lacks a size assertion. Without the bound in xasm's
output, that tool must re-derive 6502 dataflow itself. The first
implementation did so with regular expressions over the routine's source text,
which has two structural failures:

- **Symbolic masks are invisible.** `AND #$07` matches a `#\$07` regex;
  `AND #PROJECTILE_DAMAGE_SELECTOR_MASK` (which assembles to the same byte)
  does not. Coverage therefore *shrinks* precisely as a disassembly matures and
  symbolizes its literals — the opposite of what is wanted.
- **Bounds are mis-attributed in multi-loop routines.** A routine that copies
  several small tables each has its own `CPY #$04` loop. Matching "the routine
  contains `CPY #$04`" cannot tie the compare to the specific read site, so any
  same-sized table read anywhere in the routine is falsely reported as bounded.

Both are eliminated by performing the analysis in xasm, which already has
decoded instructions with **symbol-resolved immediates** (`immediate_value`)
and a **barrier-aware instruction window** (`collect_window_instruction_indexes`)
that does not leak across labels or routines.

## Proposed Solution

Add two fields to each `--analyze-index-patterns` record:

- `index_upper_bound` (integer): the proven exclusive upper bound (element
  count) on the index register at this access site.
- `index_bound_kind` (string): `"mask"` or `"compare"`.

Both are emitted only when a bound is proven. Records without a proven bound
are unchanged, so existing consumers are unaffected (backward compatible).

The additive form is preferred here only because it is sufficient. Existing
`--analyze-index-patterns` fields and even the option's output shape may be
changed if that simplifies consumers or reduces work, provided the `nesrev`
process scripts that read the artifact are updated in the same change.

## Invocation Budget

Assembling a large banked project (e.g. kid_icarus) costs several seconds per
xasm run, so per-pass invocations must be minimized. Two rules follow:

- **This feature adds zero xasm invocations.** `index_upper_bound` /
  `index_bound_kind` ride inside the `--analyze-index-patterns` output that
  `project-pass-prep` already generates as part of its single bundled
  analysis/compare invocation (alongside `--data-consumers`, `--xref`,
  `--compare`, `--analyze-data-coverage`). No new option is introduced and no
  new pass is run.
- **The consumer never assembles.** `data_extent_missing_scan` reads only the
  cached `index_patterns.json` (and, for the table span, `data_consumers.json`)
  produced by that bundle. It must not shell out to xasm. If the cache is
  absent (a standalone invocation with no prior `project-pass-prep`), the scan
  skips rather than assembling.

If a future need would otherwise require a second artifact or a second
invocation, prefer instead to widen the existing bundled outputs — for example,
carrying the table's `declared_size` onto the index-pattern record so the
detector needs a single file — rather than adding an xasm run.

### Detection

Anchored at each indexed-access site (an `index_instr` whose `index_register`
is `X` or `Y`):

- **Mask idiom.** The index register is written by `TAX`/`TAY` whose input
  accumulator was `AND #imm` with `imm == 2^k - 1`. Bound = `imm + 1`.
  Example: `AND #$03 / TAY / LDA T,Y` → bound 4. Contiguous `ASL A` scaling
  between the `AND` and the transfer is skipped, consistent with the existing
  `count_contiguous_asl_a_before_transfer` scaled-accumulator detection.
- **Compare idiom (loop counter).** Scanning forward from the read, an
  increment of the index register (`INX`/`INY`) followed by a `CPX`/`CPY #imm`
  on that register — the loop counter/bound pair — gives bound = `imm`.
  Example: `LDA T,X / STA dst,X / INX / CPX #$10` → bound 16. The terminating
  compare of a *scan* loop sits past its early-exit branch, so in-loop
  conditional branches and `CMP` are stepped over; example:
  `LDA T,X / CMP val / BEQ found / INX / CPX #$05` → bound 5. The forward scan
  stops at a segment barrier, a routine-boundary label, a `JSR` or unconditional
  transfer (the loop's back-edge/exit), or a reload of the index register.
  Requiring the increment before the compare ties the bound to *this* read's
  loop and rejects a compare that merely guards the access without counting it
  (e.g. a nearby `CPX #N` on a value the read does not iterate).

Because immediates come from the assembled instruction, symbolic constants
resolve for free: `AND #PROJECTILE_DAMAGE_SELECTOR_MASK` and `AND #$07` both
yield 7 → bound 8. Both scans are tied to the read site's own routine and index
register, so bounds do not leak across scopes or onto an unrelated same-count
loop. The bound is register-tied: a mask or compare on a different register than
the read yields no bound.

### Non-goals and limitations

- Only the two direct idioms are recognized. A masked value that reaches the
  index register through a store and reload, a loop bounded by `DEX`/`DEY` (no
  compare), a compare that guards the read from *before* it, or a bound held in
  a variable, is reported as *no bound* rather than a wrong bound. Under-reporting
  is preferred to mis-reporting.
- `index_upper_bound` is a necessary signal, not a guarantee that the table is
  exactly that size. Consumers compare it against the table's declared span to
  decide (e.g. bound == declared_size ⇒ a size assertion is warranted).

### Implementation notes

- New `determine_index_upper_bound(ctx, instr, &bound_kind)` in `listing.c`,
  mirroring `determine_index_source_kind`'s register-write backtrace and
  reusing `collect_window_instruction_indexes` and `writes_register`.
- Two new fields on `index_pattern_record`; set in `analyze_index_site_pattern`
  next to `source_kind`, populated where the record is built.
- Emitted in the JSON writer next to `index_value_source_kind`, and in the
  text formatter alongside `estimated_record_width`.
- Mnemonic constants (`AND_MNEMONIC`, `CPX_MNEMONIC`, `CPY_MNEMONIC`,
  `TAX_MNEMONIC`, `TAY_MNEMONIC`, `ASL_MNEMONIC`) already exist in `astnode.h`.
- Additive analysis only: no change to code generation or assembled output.

### JSON example

Before:

```json
{
  "table_label": "FinalStagePatchDefaultAttributeTable",
  "routine": "FillFinalStageScrollAttributeBuffer",
  "index_register": "Y",
  "index_value_source_kind": "register"
}
```

After (mask idiom, `AND #$03 / TAY / LDA …,Y`):

```json
{
  "table_label": "FinalStagePatchDefaultAttributeTable",
  "routine": "FillFinalStageScrollAttributeBuffer",
  "index_register": "Y",
  "index_value_source_kind": "register",
  "index_upper_bound": 4,
  "index_bound_kind": "mask"
}
```

## Downstream consumer

`nesrev`'s `data_extent_missing_scan` reads `index_patterns.json` together with
`data_consumers.json` (both already generated by `project-pass-prep`): for each
read site whose `index_bound_kind` is `mask` or `compare` and whose
`index_upper_bound` equals the table's `declared_size`, and for which the
project has no `data_extent_assertions.csv` row, it emits an advisory
candidate. This deletes the Python-side regex bound-proof entirely and fixes
the symbolic-mask and multi-loop failures at the source.

## Testing

- xorcyst fixtures under `tests/` covering:
  - mask idiom with a raw immediate (`AND #$03 / TAY / LDA T,Y`),
  - mask idiom with a symbolic immediate (`MASK EQU $03` / `AND #MASK / TAY`),
  - compare loop (`LDA T,X / INX / CPX #$10`),
  - scan loop with an early-exit branch (`LDA T,X / CMP / BEQ / INX / CPX #$05`)
    ⇒ bound past the in-loop branch,
  - compare without an increment (`LDA T,X / STA / CPX #$06`) ⇒ no bound,
  - register mismatch (`AND #$03 / TAX` but read via `Y`) ⇒ no bound,
  - no-bound control (`LDY $nn / LDA T,Y`) ⇒ no bound.
  Assert the emitted `index_upper_bound` / `index_bound_kind`. Extend
  `tests/regression.sh`.
- Confirm the assembled binary and all other outputs are byte-identical
  (analysis-only change).

## Acceptance Criteria

- `--analyze-index-patterns` emits `index_upper_bound` + `index_bound_kind` for
  the mask and compare idioms, resolving symbolic immediates.
- Register-tied: a mask/compare on a register other than the read's index
  register produces no bound.
- Barrier-scoped: bounds never leak across labels or routines.
- Backward compatible: records without a proven bound are unchanged; existing
  consumers are unaffected.
- Zero additional xasm invocations per pass: the fields ride in the existing
  bundled `--analyze-index-patterns` output; the consumer reads cached
  artifacts only and never assembles.
- xorcyst regression suite passes; nesrev assembly/parity is unaffected.
