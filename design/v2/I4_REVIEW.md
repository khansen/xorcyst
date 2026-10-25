# I4 local reviewer prompt

Status: **local independent review cleared `cf14315` with no blockers; I4
owner approval is pending.** The prompt below is retained as the review scope.
The optional leading-RAM initializer `$` limitation is tracked in TODO and the
validation record; this review does not approve it as a contract exception.
[I4_VALIDATION.md](I4_VALIDATION.md) and [i4-current.json](i4-current.json)
record current evidence separately from earlier measurements.
This is a focused re-review after `02264e0`. That review cleared label pruning
and reassignment, but found early export capture suppressing unused-equate
warnings. It also identified two older RAM differences. This revision restores
W0201/Werror and takes the v1-compatible option for both RAM cases, under the
existing placement policy. Review the latest section of the validation record.
No stricter RAM policy, additional language feature or wire change is proposed.
The reviewed runtime revision is `cf14315`; subsequent status edits are
documentation only. The manifest and the owner's bounded-cost decisions are
not implementation approval records.

Please review the local `feat/v2-objects` worktree at
`/private/tmp/xorcyst-v2-objects` against merged I3 `cb98568`.
Use scratch copies for builds and probes; do not modify the original projects,
frozen assets or baseline archive, and do not commit, push or make GitHub writes.

Read the [design](../../XORCYST_V2_DESIGN.md),
[contracts](../../XORCYST_V2_CONTRACTS.md),
[wire draft](OBJECT_WIRE_DRAFT.md), [performance protocol](PERFORMANCE.md),
[implementation notes](../../v2/README.md) and validation record. This stage
adds XOROBJ2 object assembly, a checked reader and the isolated `xlnk-v2`, using
the shared value/layout backend. Installed tools stay v1. V1 object compatibility
and I5 analysis/consumer migration are outside this stage.

## Explicit owner-accepted memory tradeoff

On 2026-10-25 the owner accepted the current measured memory overhead for now.
These are mostly small NES programs; a small absolute cost is acceptable when
we can explain what it buys. The design and contract section 12 record the
decision. It is not I4 approval or a claim that every allocation is necessary.

The reviewed candidate-14 standalone-link session reported:

| Project | v1 peak RSS | v2 peak RSS | Additional bytes | Ratio |
| --- | ---: | ---: | ---: | ---: |
| dpadhero2 | 2,637,824 | 3,391,488 | 753,664 | 1.2857 |
| DPadHero3Proto | 2,080,768 | 2,719,744 | 638,976 | 1.3071 |

The DPadHero3Proto measurement exceeds the earlier below-30% limit and is
explicitly accepted as the current tradeoff. Do not treat this documented excess
alone as a blocker, or require extra machinery merely to cross that percentage.
Continue reporting each tool's absolute peak and ratio; a complete-build peak
does not explain away a standalone tool's cost. The acceptance is not a new
general budget for further growth or unmeasured configurations.

V1 walks compact bytecode. V2 retains layout records, shared expression values,
cache/dependency state, definition/address identities and source spans for
located diagnostics. Instrumented requested storage after dpadhero2 layout is
1,116,430 bytes versus 546,440 for v1. Of the v2 total, layout/placement uses
366,240 bytes, expression/cache/dependency/workspace storage 361,216,
definitions/anchors 118,305, and source spans/names 100,911. Owned raw bytes and
other bookkeeping account for the rest. Full output buffers and full analysis
fact arrays consume zero bytes in this byte-only link. The validation record
explains the instrumentation's scope; these allocated-capacity figures are
not an exact partition of peak RSS or timing measurements.

Please independently assess where the cost goes and whether the implementation
is appropriately simple. Look for unnecessary copies, redundant retained state,
oversized capacity, avoidable workspaces, and opportunities to release temporary
storage earlier. Check whether the existing custom heap and packed records earn
their complexity; they are implementation choices, not contract requirements.
Suggest low-effort improvements with measured benefit where possible. Prefer
maintainable changes over elaborate allocators or encodings that save only small
amounts. No optimization should compromise ownership, sanitizer visibility,
allocation-failure handling, diagnostics or the no-slowdown requirement.

Leaks, unsafe lifetimes, pathological growth and unnecessary complexity remain
valid findings. Separate those from optional savings within the accepted cost.
The owner has not waived byte equality, safety checks or independent review.

## Bounded timing work and absolute impact

The owner subsequently clarified that single-digit-millisecond increases do
not justify prolonged optimization; hundreds or thousands of milliseconds do
warrant investigation. The candidate-14 formal samples show paired median
elapsed deltas of +0.118 ms for Bombsweeper and +0.619 ms for DPadHero3Proto
(CPU +0.119/+0.601 ms). Their ratios still exceed 1.0; these specific small
costs may proceed to review without another percentage-driven tuning cycle.
The policy and raw evidence remain visible, rather than being relabeled as
strict no-slowdown passes.

Review absolute per-stage and complete-build costs. A five-pair survey across
the object projects is intended to find material regressions with bounded
effort; it does not prove sub-millisecond equality. Check for accumulated
cost across many commands and request focused evidence for substantial
slowdowns. Large-project and analysis/bundle requirements remain in force.
Suggest simple improvements when worthwhile; do not demand elaborate machinery
to shave a small measured absolute cost. Contract section 13 records this
owner guidance separately from implementation approval.

## Correctness and evidence to check

- Prioritize W0201 and `--Werror=unused-equ` for exports, private dependencies
  and forward chains. Early export capture must retain label references while
  preserving constant source-use counts. Werror must preserve the old output;
  a genuine source use must still suppress the warning. Check snapshot cleanup
  and that default builds pay no constant-list allocation cost.
- With no script RAM blocks, check sizes as before but leave addresses unplaced.
  Unused RAM and unused exports must link; a local or imported address use must
  fail. Unresolved RAM counts and exhausted supplied blocks must still fail.
  This is a placement exception, not permission to skip extent validation.
- Leading unnamed `.dsb` counts, including `$+1`, are discarded as in v1.
  Storage after a surviving label still contributes to its size. Distinguish
  discarded storage counts from RAM initializer values that still need checks.
- For this follow-up, prioritize assignments whose RHS captures their previous
  deferred definition: successive uses must retain 5, 6 and 12 for a provider
  value of 5. Cover forward local values, addresses, imports, aliases and strings.
  Check the cache generation advances after definition replacement, and that
  the shared v1 build remains unaffected by the guarded changes.
- Check export capture occurs after public tagging and before label pruning.
  Private labels referenced only by an exported alias must survive, including
  arithmetic/low-byte/difference and RAM aliases. Other unused labels still
  warn and disappear; preserving them all would change RAM placement.
- Check leading unnamed `.dsb` storage (including after removed labels) gets no
  allocation identity, but storage after a surviving RAM label still contributes
  its extent. A retained `$` use must not lose its required anchor. Writer failures
  without location metadata must print `location unavailable` and keep outputs.
- Validate wire bounds/tags and malformed-object handling independently of the
  writer; check the unchanged 244-byte example and reserved tags. Tag 8 counts
  one element for a number and byte length for a string, as owner-approved.
- Exercise imports/exports, implicit externals, strings and captured maps,
  initializer extents, RAM order/first-fit, banks/pads/copy, narrowing order,
  range policy and preserved source bindings. Use v1 or contract oracles rather
  than assuming reader/writer agreement proves correctness.
- Review ownership across decoding, translation, binding, layout and emission,
  including shared backend changes. Check both query modes, failure injection,
  zero retained allocations, output aliasing and staged publication failures.
- Check that missing/directory copies, oversized origins and allocation failures
  report a real error and preserve previous outputs. Undefined imports and
  duplicate exports must name the symbol. Strict numeric parsing is now an
  owner-approved compatibility correction, recorded in the ledger and NEWS.
- Review reference/address leaf sharing and absent leaf spans, including cycle
  proxies and retained definition/anchor/fixup locations. Literal, operator,
  bank and RAM-wrapper spans remain. I5 requires direct-binary analysis, not
  per-term provenance from objects. Check this boundary against contract section 6.
- Exercise the factored word-conversion helper and the bank-target lookup. The
  conversion test exercises the production loop on a little-endian host; it is
  not a claim of running on big-endian hardware. The binary-mode mapped forward
  initializer control must emit `01 00 07`, without I3's stale padding byte.
- Reproduce required ROM equality and diagnostics from source-built objects.
  dpadhero2, ntxdev, bombsweeper and all 12 music fixtures are required. Keep
  frozen converter outputs and converter traps; no old objects or stale ROMs.
- Verify performance evidence uses matched builds, fresh outputs, serial paired
  sampling and per-stage results. Keep failed and inconclusive sessions visible.
  Review the source/tool hashes and distinguish evidence from older candidates.

Current evidence is under `/private/tmp/xorcyst-i4/`. All 15 ROMs and stderr
match from 451 source-built objects. There are 38 object and 48 frontend tests;
the current full check/distcheck runs pass 535/540 tests with no skips. Linux GCC
13.3/aarch64 passes values 19, layout 31, frontend 48 and objects 38 under
ASan/UBSan/LeakSanitizer, plus strict compilation of all seven modules. The object
and frontend suites also pass under macOS ASan/UBSan. Current Metroid/Zelda PRGs
and stderr match v1 in a separate correctness run. Three new/extended controls
fail 23 assertions on `02264e0`, with no harness errors. Earlier regression
controls, including the preceding 33-assertion parent proof, remain in the suite.

The actual writer reduces dpadhero2 objects from 1,116,264 to 972,884 bytes
(12.8%), and DPadHero3Proto from 466,764 to 389,128 (16.6%). The remaining
5.1x/9.5x versus v1 is not required by the language. Fixed-width tables simplify
checked reading/native table access; redundant leaf metadata has been removed.
No variable-length codec or wire redesign is proposed. The validation record
lists artifact sizes for all projects and the reviewer's translation-peak trace:
the raw largest object and translation workspace overlap with retained state.
The post-layout allocation accounting above does not partition peak RSS.

The reviewed `42ff0c0` five-pair survey of dpadhero2 and DPadHero3Proto reports linker
median elapsed times of 9.12→8.83 ms and 5.15→5.51 ms. Paired median deltas
are −0.32/+0.09 ms. Peak link RSS is 2,637,824→3,325,952 and
2,048,000→2,654,208 bytes. This is bounded cost evidence, not a formal gate or
an isolated measurement of the leaf change; do not infer a precise speedup.
This follow-up changes the frontend warning bookkeeping, the layout allocation
early return and the writer's unnamed-RAM handling. All project artifact sizes
are unchanged; no new formal performance session is claimed.

Historical candidate-14 evidence remains attributed to `6d91699`. Its bounded
object survey covers all 45 project/stage combinations;
its largest positive link elapsed delta is +0.562 ms, and all complete-build
medians improve. Read the validation record for exact absolute values, session
limits and per-tool RSS, including runs above the original 30% limit. The earlier
formal link session remains evidence and is not overwritten by the short survey.
Its 15-pair plain Metroid/Zelda session gives elapsed ratios 0.9186/0.8304,
CPU ratios 0.9346/0.8461 and peak-RSS ratios 0.8919/0.8539, with identical PRGs
and stderr. This is one serial formal session, not independent confirmation.
No real 32-bit or big-endian host has been tested. Do not infer missing gates
from a targeted suite or an older runtime's results.

Report blockers separately from optional improvements, with reproductions,
expected results and evidence. Identify any scope or compatibility change that
needs an owner decision. A clear review is distinct from owner approval and
publication authorization.
