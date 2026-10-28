# Development strategy after 2.0

Maintainer guidance distilled from the 2.0 implementation and independent
reviews. This document concerns future 2.x work. Existing semantic decisions
remain in [the contract](XORCYST_V2_CONTRACTS.md) and resource policy in
[the performance protocol](doc/performance.md). The bounded generated-program differential is implemented in
[tests/differential/](tests/differential/README.md); portable fault-tool integration
remains future work.
This source-only document is not installed or included in the release archive.

## Start with evidence and a cost model

Tie a change to a reproduced defect, an existing project dependency or an
explicit owner request. Identify the source behavior and consumers affected
before choosing an implementation. Prefer the smallest change that satisfies
that need. A synthetic case can expose a general correctness defect, but does
not by itself justify a new feature or a substantial optimization project.

For a representation or format change, first measure a small prototype on
representative project inputs. Report artifact sizes, retained allocations,
transient peak memory and elapsed/CPU costs. Explain what each significant
category buys. The fixed-width object format's size and translation-buffer
overlap should have been quantified before implementation, rather than first
explained during I4 review.

Report absolute bytes and milliseconds alongside ratios. The owner's bounded
effort and small-cost decisions are requirements, not failed optimization
targets. Follow the existing performance policy: investigate material
regressions and cumulative build costs; avoid elaborate machinery or repeated
long campaigns solely to eliminate tiny absolute differences.

## Make generated differential testing part of author validation

The most productive review technique was generated programs compared against
the baseline across outputs and options, together with allocation-failure
injection. Fixed fixtures and targeted mutants remain valuable, but largely
exercise behavior the author already thought to test. Independent review should
add another perspective rather than supply the first broad differential run.

The removed legacy engine does not make the v1 oracle unavailable. Preserve the
frozen 1.9.12 sources, binaries and reviewed generators outside scratch storage.
Record revisions, executable hashes, compiler flags, host requirements, seeds
and invocation details. Keep generated reproducers, not only their seeds.

For future 2.x comparisons, use the pinned actual 2.0.0 release commit in
`tests/differential/baseline.json`. The reference build recipe records its compiler,
flags, host and executable hashes. Retain 1.9.12 as a secondary oracle for
compatibility investigations. Approved 2.0 corrections
must not be mistaken for regressions against v1.

### Optional maintainer harness

The maintained check is documented in [the suite README](tests/differential/README.md).
It accepts explicit baseline/candidate tool directories or builds the pinned
reference, uses deterministic seeds, bounded case counts and timeouts, a minimal
environment and fresh outputs. Generated cases run without private ROMs. Its
ordinary-suite controls exercise identical tools and intentional output,
diagnostic and artifact mutations.

The optional command runs two generated checks: 100 generated
pure-binary programs across every supported analysis product/format, and 100
generated multi-unit object/link programs. Use fixed, recorded seeds and a
five-minute wall-clock budget on the recorded maintainer machine, excluding
tool compilation. This is an initial target to measure, not an existing timing
result. Report actual programs, invocations and elapsed time. If the deadline
or a case timeout prevents completion, mark the run incomplete and identify
what was omitted; do not silently shrink the matrix to claim a pass. Keep
larger campaigns separately selectable.

The maintained suite adapts the I6-era `adiff56.py` and `linki56.py`. Their
originals omit the removed listing selector, but retain v1/v2 terminology and
prototype baseline names in the linker runner; the adaptation uses normal tool
names throughout. The earlier analysis set includes
`agen.py`, `adiff.py`, `selfcheck.py`, `injsweep.py`, `stagetest.py` and
`diagasan.py`, with `failalloc.c` and `failfs.c` interposers. The earlier linker
set includes `linkdiff.py`, `objfuzz2.py` and `assigngen.py`. Also retain
`ddmin.py` for input reduction, `jdiff.py` for structural JSON differences and
`injbt.sh` for allocation backtraces, together with the original injection and
publication fixtures and the I6 generator dependency.

Keep their original bytes, provenance and checksums in the external evidence
archive. They contain review-specific paths, feature exclusions and, in the
older versions, obsolete CLI assumptions. Audit their normalization before reuse.
The interposers and backtrace script use macOS DYLD. A portable integration
needs Linux equivalents such as `LD_PRELOAD` or the existing test hooks, with
separate validation.

| Surface | Compare or validate |
| --- | --- |
| Source binding and layout | Reassignment, `.undef`, aliases, local/anonymous labels, types, expansion, `$`, `sizeof`, segment switches and narrowing; include interactions between them |
| Direct binary and object/link paths | Each version builds its own objects; compare final bytes, acceptance, warnings and errors across imports/exports, RAM alignment, banks, pads, copies and unit order |
| Analysis and listings | Every supported product/format, relevant filters, debug/case options, and combinations such as listing plus FCEUX export; compare contents as well as status |
| Failure and publication paths | Existing, absent and aliased destinations; output preservation, temporary-file cleanup, backup/restore behavior, and allowed diagnostic-only publication |
| CLI and installation | Help, usage, version, invalid options, exit codes, generated man pages and installed behavior |

Compare exit status, stdout/stderr, diagnostic names and locations, warning
counts, artifact presence and contents. Assert expected row counts and required
fields before checking per-row properties: `all(...)` on an empty result proves
nothing about a listing that should contain statements. Validate listing bytes,
instruction operands and data references against the emitted binary too.

Normalize only documented incidental differences such as timestamps and scratch
path prefixes. Keep identities, locations, missing fields and missing rows
visible. Record each intentional difference with a narrow reproducer and its
approved expectation. Classify unexpected differences as regressions, inherited
defects, harness defects or proposed semantic changes; equality alone is not
proof of correctness. Do not cross-read incompatible v1/v2 object formats.

On a mismatch, retain both raw runs and reduce the input. Add the minimal case
to the ordinary suite; use a parent proof or a targeted mutant when it helps
show that the assertion detects the defect.

Every differential report must include coverage of the executed workload:

- Generated, attempted, completed and timed-out case/invocation counts, by path
  and product/option configuration.
- Baseline and candidate acceptance rates, their acceptance disagreements, and
  rejection reasons. Product comparisons need their own successfully emitted
  artifact counts; rejected programs still exercise diagnostics.
- Feature counts in generated programs and in accepted programs, including
  important combinations. Report what appeared, not merely what the generator
  was capable of producing.
- Every excluded feature/category, its reason and any approved-difference
  reference. The review runs sometimes disabled `.char`, string data or
  `.org $+N`; these exclusions must remain visible. Keep focused controls for
  approved differences rather than silently dropping whole categories.

The reviewer observed roughly 25–40% rejection in some generated workloads.
An all-equal result can therefore cover little successful output. Report those
denominators and uncovered categories rather than presenting equality as broad
coverage.

Run the 200-program check before local review of runtime changes. Broaden the
option combinations and failure cases for shared binding, evaluation, layout or
publication changes and for release cutovers. Identify the actual selected
checks and their gaps, including incomplete runs or omitted configurations.
Longer campaigns belong to risky changes or an observed failure;
documentation-only edits do not need them.

Existing checks to reuse:

- [Project checks](tests/projects/README.md) cover frozen ROMs and performance
  surveys; `tests/verify_fceux_projects.py` already compares real-source binary,
  diagnostics and analysis products across several modes.
- `tests/test_output_failures.py` and the IR allocation/ownership harnesses
  provide fault-injection controls. Keep test hooks out of production builds.
- The generated-program differential now covers analysis options and multi-unit
  linking; its reports distinguish rejected cases from emitted products. The
  project ROM gate alone does not provide it. A timing script's built-in threshold
  is not a substitute for the owner-approved performance policy.

### Integration plan and completion criteria

The bounded differential, reducer, coverage report, pinned reference recipe and
Linux/macOS workflow wiring are implemented in `tests/differential/`. The
[provenance map](tests/differential/README.md#provenance-and-remaining-integration)
identifies the preserved inputs and their maintained replacements. Normal runs
never read the external evidence archive. The remaining integration work is:

1. Validate the configured PR workflow on fresh Linux/macOS hosts, retaining
   coverage, failure bundles and reference-build receipts. Ordinary `make check`
   must remain independent of a reference build.
2. Adapt allocation, publication, backtrace and object-mutation tools into the
   existing fault and object suites where practical. Track synthetic fixtures.
   Prefer existing portable test hooks; validate any necessary platform-specific
   interposers separately. Preserve exact path/destination conditions needed to
   reproduce failures, and keep expensive sweeps opt-in.
3. Update the provenance map when the remaining tools have maintained replacements
   or are explicitly superseded. Keep the original snapshot for provenance.
   Mark the broader TODO complete only when the declared checks run from fresh
   Linux/macOS checkouts without private projects, scratchpads or owner-local files.

For later 2.x releases, retain the pinned 2.0 reference for cumulative behavior
checks and compare with the immediate base for changes under review. Update
baseline pins and intentional-difference controls through review; do not move
the baseline merely to make a failing comparison disappear. Minimized regressions
belong in the ordinary test suite so they survive changes to generators or seeds.

## Check the behavior around every fix

A fix can satisfy its repro and pass the existing suite while introducing a
different regression. Choose neighboring checks from the state it changes:

| Change seen during 2.0 | Adjacent behavior to check |
| --- | --- |
| Retaining exported-alias dependencies | Reference counts, unused-equate warnings and `--Werror=unused-equ`, as well as successful linking |
| Skipping frontend work after syntax errors | Diagnostic listings with symbolic data and FCEUX export, including preserved previous outputs |
| Promoting prototype tools | Full CLI/help/man-page and diagnostic parity, not only bytes and analysis products |
| Changing capture or guard timing | Same-line/later-line operands, reassignment, anonymous labels, typed-data replacement and ownership through expansion |

For repeated findings in one area, stop applying isolated patches and write down
the model: when names bind, when widths are captured, when guards run, and which
objects survive each pass. Test the combinations of those states before another
patch. The `sizeof` churn and freed user-type statement show why a lifetime
assumption needs verification in the code.

Treat failure paths as part of the product. Sweep reached allocation failures
through parsing, evaluation, collection and serialization, then exercise writes,
backup links, publication renames and restoration. Check the entire output set.
Do not infer post-publication safety from a pre-publication sweep. Allocation
indices can change between builds; compare failure invariants and record phase
coverage rather than requiring equal index numbers. To reproduce a particular
index, fix the executable/interposer, arguments, environment, exact working and
output paths (including their lengths), and initial destination state. Record
whether each output was absent, a file or a symlink, with its contents/target;
restore that state before each injection. The review found that path length and
pre-existing outputs alone changed allocation indices. Record the no-fault
allocation count too. Report custom-allocator sanitizer blind spots and
supplement sanitizers with ownership/live-count checks.

## Make every validation claim match its evidence

Distinguish introduced regressions from inherited behavior. Parser allocation
handling and publication order exposed during 2.0 partly came from v1; finding
them late does not make the new engine their origin. An inherited defect still
needs an honest limitation entry or a fix when it violates the current contract.

Use a concise review record containing:

- Exact tested revision, executable hashes and any uncommitted test patch.
- Commands, products/options, seeds/case counts and fault phases exercised.
- Acceptance rates, exercised feature counts and all excluded categories.
- What was asserted: bytes, required facts, diagnostics including locations,
  preservation, resource limits or some stated subset.
- Platforms/instrumentation, failures, known differences and untested scope.
- Absolute and relative resource results, with survey/formal-session status.

Say "the warning points at the alias definition" when that is what was checked.
Say which allocation phases preserved outputs. Keep reviewer evidence separate
from author-run checks. A green suite, zero observed mismatches or zero sanitizer
reports is evidence for the exercised scope, not a universal guarantee.

## Keep review and release work small and traceable

Finish author validation, obtain local independent review, resolve findings and
obtain owner approval before publishing a PR. CI follows publication; merge only
when the required checks pass. Keep installation, consumer activation, tagging
and release publication as explicit coordination steps.

Develop shared tooling from the relevant public mainline. Keep private workflow
integration separate and respect each repository's publication boundaries.
Record repository-specific branch and activation instructions in its release
checklist or local documentation. Pin consumers to the actual merged producer
commit and verify the live workflow before switching its installed tools.

Use one maintained strategy, the current contracts and a short review record.
Keep raw evidence in a durable external archive keyed by revisions and hashes;
completed committed records remain in Git history. Clean up superseded branches
and worktrees after preserving unique work. Keep TODO for worthwhile active
priorities and known limitations for deferred or low-value issues.

Separate the tested implementation revision from later documentation edits and
the packaged archive identity. After shipped files change, commit the edits,
rebuild the archive and verify its contents/checksum. State which earlier test
results still apply and why; do not relabel them as a fresh run. Refresh only the
affected evidence instead of repeating every campaign or rebuilding a large
stage-document inventory.
