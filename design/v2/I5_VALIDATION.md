# I5 local validation

Status: independent local review cleared `63e4ea5` / Nesrev `4ca95e06d` with
no blockers. Owner approval remains pending; no PR has been opened. The first
review of `21fae67` / `ab50ea8f7` requested changes; the follow-up closes B1/B2
and S1–S6. This subsequent revision changes documentation and evidence metadata
only. Runtime, tests and tools are unchanged, so no new test run is claimed.
`i5-current.json` identifies the files, tools and evidence. Installed tools,
original project trees and frozen inputs remain unchanged.

## Independent follow-up review

The reviewer independently confirmed all 52 product configurations, all 15 ROMs
and 451 unchanged objects, and all eight bundle profiles for both games. Their
68 Nesrev shell cases and eight Python suites passed with v1 and the candidate.
The sanitizer run included 829 well-formed diagnostic listings from 200 mutated
programs, plus 113 accepted programs across 18 product configurations, without
reports. These are reviewer results, separate from the author runs below.

Their publication checks failed every rename and backup link in ten-file and
seven-file output sets. Existing, absent and symlinked destinations survived;
failed restores reported and retained the backup. The late evaluator allocation
failure now exits 3 without publication. Comparison and audit findings retain
their existing artifact publication. The review reports no introduced blockers.

Two optional observations are now explicit in contract section 15:

- With `.org $8000 / L: nop / .byte <L / bad bad bad`, requesting both a text
  listing and FCEUX NL ROM files can prevent destination planning. The tool
  preserves the previous listing with "could not collect assembly analysis".
  v1 published a diagnostic listing; this conservative refusal is allowed.
- An unused `K .equ ^L` reports `null` and has no text symbol row, whereas v1
  invents value 0 and prints `K = $0000`. Assembly acceptance is unchanged.

The review also identified inherited failures that the earlier record omitted:

| Shared collector under allocation failure | Reviewer-observed failure indices | Behavior |
| --- | ---: | --- |
| Summary | 76, same as v1 | Can exit 0 with incomplete output |
| Text-listing scope/source text | 12, versus v1's 42 | Can exit 0 with incomplete output |
| Xref data provenance | 9, same as v1 | Can exit 0 with incomplete output |
| Index, consumers, coverage | not enumerated here | Exit 7–9 without an explanatory message; outputs are preserved |

These are observed injection counts, not total allocation counts or exhaustive
coverage. Staging protects against detected failures; it cannot detect silently
ignored collector errors. They remain open alongside shared parser allocation
failures, and this record does not waive section 6's fail-closed requirement.
Since I4, `L: nop / K .equ L+1 / J .equ K+1` also suppresses v1's unused-L
warning; no new correction is claimed here.

The reviewer's 15-sample alternating timing run measured Metroid combined xref
at +17.8 ms minimum and +16.8 ms median versus v1, compared with the author's
five-pair +11.86 ms median survey below. Plain Metroid is +1.9 ms over I4 but
still about 9% faster than v1. Staging costs about 0.3 ms with an existing
destination in plain and object modes. These are small measured costs under
the owner's existing bounded-effort decision, not a formal performance pass.
The text shared-alias case used 2.3 MiB and 4 ms; large synthetic inputs were
1.28–1.29× v1. The reviewed projects remained below the memory gate.

Reviewer evidence is in
`/private/tmp/claude-501/-Users-kenthansen-dev-github-xorcyst/0ef1b090-a72f-41b3-b907-3b7d795327cb/scratchpad/i5.zymPZV/`
(`c2/`, `c2asan/`, `logs/*c2*`, `logs/perf2.log`, and `scripts/`). The reviewer
did not rerun check/distcheck, Linux, native 32-bit/big-endian, the full Nesrev
suite, an emulator, the formal timing protocol or sanitizer allocation injection.
The earlier author results below remain attributed to their actual runs.

## Review findings addressed

- **B1:** standalone JSON/NDJSON diagnostic listings retain their source rows.
  Text listings contain no JSON records. `.dsw` accounts for its element width;
  first-pass instruction metadata comes from the mnemonic/mode, including
  branches and zero-page-only indexed stores. Recovered syntax-error statements
  survive. Tests assert complete row sequences, addresses and extents instead
  of accepting an empty record array. Diagnostic rows make no byte/offset claim.
- **B2:** Devil World's `render_sequences` handles non-emitting rows and compact
  fills. Tests exercise that reader independently of the three trace producers,
  and reject corrupt ordinary bytes and fills before semantic table lookups.
- **S1:** graph reference indirections no longer halve alias depth. The 32/64/65
  controls match v1; plain assembly remains independent of the collector limit.
- **S2:** literal-string `.char` grouping matches v1's lowered `.db` grouping.
  Numeric-char text bytes and declaration-position label rows remain deliberate
  final-fact corrections, now recorded in contract section 15.
- **S3/S4:** substituted strings produce no address-computation/data-target
  references. Unused bank constants remain unknown; analysis no longer changes
  their assembly acceptance. Actually used bank expressions still reject.
- **S5:** fatal evaluator allocation errors are checked after serialization.
  All file products are staged before publication; generation or dependency
  capture failures discard the stages. Publication failures roll back earlier
  renames. Existing and absent destinations, CSV/NL sets, symlinks, readonly
  directories and late value allocations are covered. This is not crash-atomic;
  stdout cannot be rolled back. A failed rollback reports and retains its backup.
  Comparison/audit findings keep their existing exit statuses and valid artifacts.
- **S6:** text-only listings use lightweight rows and the symbol snapshot, without
  rebuilding collector expression trees.

## Follow-up verification

The final 52-configuration Metroid/Zelda matrix matches v1 after the same
normalization as before: timestamps, output paths and actual producer identity
only. All 15 required ROMs rebuild exactly from 451 source-built objects. The
frozen gate and source hashes remain unchanged. Object/stderr comparisons are
recorded separately in the evidence.

`make check` passes 673 tests in 20 suites plus shell regressions. `make distcheck`
passes 678 in 21 suites and produces the distribution archives. The shared v1
fault suite has one explicit not-applicable skip for the v2-only value evaluator;
the corresponding test executes in the 117-test v2 analysis suite. The frontend
suite has 67 methods. Distribution wiring includes its new value-fault wrapper.

macOS ASan/UBSan passes the final 67-method frontend suite and the targeted late
value-allocation sweep. Linux GCC 13.3/aarch64 ASan/UBSan/LeakSanitizer passes the
117-test analysis suite and the 65-method frontend suite before the last
first-pass diagnostic-opcode and compare-result controls. The final 67-method
frontend suite passes under the same Linux sanitizers as an unprivileged user.
The three root-skipped permission subcases from the full analysis run also pass
in a separate unprivileged rerun. There are no sanitizer reports. No second full
Linux analysis run is claimed for the final diagnostic-only refinements.

The new producer controls fail 39 assertions on the reviewed `21fae67` tool,
with no harness errors. Ignoring fatal value errors and publishing sidecars
early each fail two assertions in the late-allocation test. The real code passes
both allocation positions. Ignoring Devil World's byte comparison fails both
corruption assertions; the real reader passes. These proofs are in scratch
copies, not edits to the reviewed trees. The earlier broad allocation suite
still exercises 477 collect, 3 plan, 108 prepare and 17 output-phase allocations.

The changed Nesrev reader tests pass. The rest of the consumer implementation
is unchanged from `ab50ea8f7`; no repeat of the full consumer inventory or an
emulator run is claimed in this follow-up. The fixture migration now calls out
Zelda's four final-opcode `absolute` to `zeropage` corrections.

## Bounded resource follow-up

A five-pair spot survey covers plain, text, JSON, combined xref and records on
both games. It is a survey on one machine, with other validation work active;
it is not the formal timing protocol. The largest elapsed increase is combined
xref on Metroid: +11.86 ms (+4.5%), CPU +14.39 ms. Text listing is about 4.24 ms
faster on Metroid and 8.09 ms faster on Zelda in this sample. Median peak-RSS
ratios in these ten configurations remain at or below 1.10. Under the owner's
bounded-effort decision, these small costs are reported without another round
of optimization solely to erase the percentage difference.

Three-run text-listing memory probes use the reviewer's synthetic inputs:

| Input | v1 median peak | Current median peak | Ratio |
| --- | ---: | ---: | ---: |
| Large generated source | 99.22 MiB | 126.92 MiB | 1.279 |
| Macro-heavy source | 51.39 MiB | 62.27 MiB | 1.212 |
| Shared aliases, k=22 | not repeated | 2.28 MiB | — |

The first two binaries match v1. The earlier review measured multi-gigabyte
memory for the shared-alias text path; that old run was not repeated. The new
text path is also bounded by an in-suite k=22 control. Rich xref's graph-to-tree
expansion remains an optional, documented limitation. The current survey and
artifact sizes are in `i5-survey.json` and the hashed external reports.

## Remaining limits and review scope

No PR CI, native 32-bit/big-endian run, complete Nesrev test run, emulator or
formal performance pass is claimed by the author. The shared v1 parser and
collector allocation failures described above,
rich xref's exponential alias trees, the bundle's missing full listing/binary
byte cross-check, and the pre-existing consumer `zeropage_y` omission remain
open. I4's unnamed-RAM `$` limitation remains. None is silently claimed fixed.

`I5_REVIEW.md` retains the completed review's scope. Owner approval precedes publication;
I6 still owns the coordinated cutover and removal of legacy implementations.

## Earlier candidate record: `21fae67`

The following results belong to the originally reviewed candidate, not to new
runs of every configuration in this follow-up.

### Correctness and publication

The final 52-configuration matrix covers 26 producer modes on each frozen
Metroid/Zelda input. Every PRG and diagnostic matches v1.9.12. Every analysis
product matches after excluding timestamps, output destination paths and the
actual producer identity in dependency manifests. The comparison includes full
objects/arrays, not just record counts. JSON/NDJSON listing controls use schema
2 on both producers. Bundle comparisons separately exercise the migration
from the frozen consumer's listing 1 to the migrated consumer's listing 2.

All 15 required source-built ROMs are exact, from 451 newly assembled objects.
The frozen generated assets are inputs; converter traps prevent regeneration.
Original sources and baseline archives are not build destinations. The gate
manifest remains `5eaccd61551501e36d28d495ee1a8cf8fbb1daed81291d0a2382cd0f8dfb1e30`.
The excluded precompiled-object-dependent dpadhero tree remains excluded.

Full `make check` passes 663 tests in 20 suites plus shell regressions.
`make distcheck` passes 668 in 21 suites and produces the distribution archives.
The 116-test v2 analysis suite reuses records, symbols, access classification,
FCEUX exports and output-failure tests. Approved v2 policy differences are
asserted explicitly rather than skipped. The frontend suite has 59 methods.
An additional final extension exercises analysis on the existing `.undef`
contract fixture; its focused rerun is recorded separately from the full run.

The allocation harness catches failures at 477 collection, 3 planning and 108
preparation points; output checks cover 17 xref allocation points both with
and without NL. They verify previous-output preservation and temporary-file
cleanup. These are representative exercised allocations, not a claim to inject
every possible allocation in the parser or evaluator.

macOS ASan/UBSan passed the analysis suite after ownership transfer and source
path interning. The final checked-marker/allocation cleanup is also covered by
Linux ASan/UBSan/LeakSanitizer: value 19, layout 32, frontend 59, analysis 116,
objects 38. Three restricted-directory subcases skip in the root container;
the same cases run on macOS and pass in a separate unprivileged Linux
sanitizer rerun. No sanitizer reports occurred. An exploratory
Linux build with additional `-Wextra` flags stopped at pre-existing intentional
fallthrough warnings in `astproc.c`; the CI-style `-Werror` build and sanitized
suites pass. This is not a clean claim for that extra-warning configuration.

### Consumer checks and regression proofs

Ninety focused Nesrev shell checks cover all bundle profiles, record validation,
raw/RAM address consumers, pointer audits, branch literals, split pointers,
used-by checks, static analysis and orphan-opcode scanning. The first run exposed
one missed accumulator-mode migration; the corrected static checks pass.
Additional direct suites cover bundle validation (33 selected checks), named
pointer tables (23), embedded pointers (9) and three trace byte validators
(one method with nine good/corrupted-byte/fill subcases). Trace setup uses
synthetic PRGs and does not launch an emulator.

Deliberate bad changes in scratch copies fail the intended checks:

| Mutation | Detected by |
| --- | --- |
| Reuse provenance IDs for frontend string caches | Source-capture control, one assertion |
| Drop original source spans on cloning | Source-capture control, one assertion |
| Accept a non-emitting listing row with a numeric offset | Bundle control, one assertion |
| Interpret accumulator shifts as schema-2 `implied` | Carry-confusion shell assertion |
| Stop checking trace listing bytes against the PRG | Six corruption assertions across three consumers |

Nesrev repository hygiene passes. Its strict playbook check reports ten stale
`AGENTS.md` references in existing historical review files, unchanged by this
branch. The modified tooling guide stays within its route budget. This is a
documented pre-existing check failure, not an all-green full-Nesrev-suite claim.

### Resource assessment

`measure_analysis.py` records minimal environments, tool/consumer hashes,
commands, five alternating pairs, elapsed and `wait4` CPU time, peak RSS and
artifact sizes. Each sample checks the exact frozen PRG. Two warmups precede
each configuration. Producer, full artifact loading/validation, and complete
bundle production plus loading are measured separately. Preparation is outside
timing. The baseline uses the frozen consumer; the candidate uses its migrated
consumer. Benchmarks run separately from builds/tests.

These are bounded surveys, not the formal 15-pair/two-second-batch confidence
protocol. The owner explicitly chose on 2026-10-25 to document small I5 costs
for local review and focus further work on material regressions (contract §14).
Ratios above one remain costs, even when a downstream stage improves. No
universal timing allowance, I5 approval or memory waiver is inferred.

The first full bundle measurement exposed about 143 ms extra on Metroid's
`ci-data-v1`, including about 52 ms in the reader. Hoisting immutable field/hex
tables and formatting validation errors only on failure removed repeated work;
all validation predicates remain, including explicit malformed-byte controls.
The superseded measurements remain evidence and are not mixed with final ones.

The final survey covers 100 configurations: 52 independent products and 48
bundle stages (eight profiles × two projects × three stages). All timed PRGs
are exact. The largest paired median elapsed increases are:

| Stage | Configuration | Elapsed delta | CPU delta | Elapsed ratio |
| --- | --- | ---: | ---: | ---: |
| Independent producer | Metroid combined xref | +15.31 ms | +14.13 ms | 1.0640 |
| Bundle producer | Metroid maturity-instructions-v1 | +20.30 ms | +15.37 ms | 1.0416 |
| Bundle reader/validation | Zelda process-instructions-v1 | +40.25 ms | +40.21 ms | 1.0874 |
| Complete bundle | Metroid pass-prep-instructions-v1 | +25.73 ms | +26.69 ms | 1.0073 |

The default `ci-data-v1` complete bundles have paired elapsed ratios 0.9961
(Metroid) and 0.9935 (Zelda); `ci-instructions-v1` gives 1.0007 and 0.9994.
Plain assembly ratios are 0.9169 and 0.8339. These are five-pair observations,
not confidence claims. Both per-version medians and paired ratios are retained;
the ratio of separate medians need not equal the median paired ratio.

Every observed paired RSS ratio is below 1.30. The largest is 1.1451 in bundle
reading; independent producer modes peak at 1.0945. For example, Metroid's
combined xref producer has median peaks of 86.64→90.72 MiB, while its `ci-data`
reader goes from 173.75→197.59 MiB. The reader deliberately loads and validates
all bundle artifacts together; this is not a timing of every project wrapper
or semantic audit. [i5-survey.json](i5-survey.json) records every configuration,
each version's maximum/median RSS, ratios, absolute timings and artifact sizes.

Listing migration increases the `ci-data` listing from 21,690,616 to
27,072,744 bytes for Metroid and 19,554,309 to 24,395,391 for Zelda (about 25%).
The additional fields are the existing listing-2 contract: explicit emission
state, segment, byte counts/fills, and null non-emitting offsets. No new schema
was invented for this implementation. With listing 2 selected on both tools,
the final product comparison is identical. Other product sizes are in the
survey; dependency-manifest sizes include each actual executable/path identity.

The original per-configuration below-1.30× peak-memory rule still applies to
analysis; I4's accepted small-link excess is not transferred to I5.

The retained source-row/symbol view, optional original spans, shared graph and
collector results account for the added analysis state. Native statements
transfer ownership instead of being cloned, source paths are interned, plain
assembly avoids this state, and `$` anchors are created only when needed.
The fixed AST node remains 136 bytes; rich source capture adds 32 bytes within
the same allocation. These are representation sizes, not a partition of RSS.

### Limits and next gate

No native 32-bit or big-endian host, PR CI, complete Nesrev repository test run
or emulator session is claimed. Timing comes from one macOS arm64 machine;
Linux validation uses GCC 13.3 aarch64 in Docker. Historical review artifacts
are not rewritten. I4's optional leading unnamed RAM initializer `$` limitation
is retained. The bounded analysis expression view refuses unrepresentable
graphs instead of silently omitting references; plain assembly is unaffected.

Independent local review comes next. Publication requires owner approval.
I6 still owns coordinated consumer cutover, removal of legacy engines and
prototype names/schema selectors, and the final release gates.
