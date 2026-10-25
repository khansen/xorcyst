# I5 local validation

Status: local candidate; independent review and owner approval are pending.
No I5 PR or installed-tool cutover is claimed. The producer is based on I4
merge `63747ed`; the isolated Nesrev migration is based on `abc8ae3e1`.
`i5-current.json` identifies the tested source, tools and external evidence.
The review scope is in [I5_REVIEW.md](I5_REVIEW.md).

## Correctness and publication

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

## Consumer checks and regression proofs

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

## Resource assessment

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

## Limits and next gate

No native 32-bit or big-endian host, PR CI, complete Nesrev repository test run
or emulator session is claimed. Timing comes from one macOS arm64 machine;
Linux validation uses GCC 13.3 aarch64 in Docker. Historical review artifacts
are not rewritten. I4's optional leading unnamed RAM initializer `$` limitation
is retained. The bounded analysis expression view refuses unrepresentable
graphs instead of silently omitting references; plain assembly is unaffected.

Independent local review comes next. Publication requires owner approval.
I6 still owns coordinated consumer cutover, removal of legacy engines and
prototype names/schema selectors, and the final release gates.
