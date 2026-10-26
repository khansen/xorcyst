# Development strategy after 2.0

Maintainer guidance distilled from the 2.0 implementation and independent
reviews. This document concerns future 2.x work. Existing semantic decisions
remain in [the contract](XORCYST_V2_CONTRACTS.md), resource policy in
[the performance protocol](doc/performance.md), and release coordination in
`RELEASE_CHECKLIST.md`. The differential harness proposed below is future work.
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

For future 2.x comparisons, freeze a reference build from the actual 2.0.0
release commit once it is published. Before publication, identify any reference
by its exact candidate commit; do not call it a released baseline. Retain 1.9.12
as a secondary oracle for compatibility investigations. Approved 2.0 corrections
must not be mistaken for regressions against v1.

### Planned optional maintainer harness

Adapt the useful reviewed generators into a small, reproducible check under
`tests/`. Use explicit baseline/candidate tool paths, deterministic seeds,
bounded case counts and timeouts, a minimal environment, and fresh scratch
outputs. Generated cases should run without private ROMs. Port and test the
generators before treating their output as trusted evidence.

The first deliverable is one optional command with two checks: 100 generated
pure-binary programs across every supported analysis product/format, and 100
generated multi-unit object/link programs. Use fixed, recorded seeds and a
five-minute wall-clock budget on the recorded maintainer machine, excluding
tool compilation. This is an initial target to measure, not an existing timing
result. Report actual programs, invocations and elapsed time. If the deadline
or a case timeout prevents completion, mark the run incomplete and identify
what was omitted; do not silently shrink the matrix to claim a pass. Keep
larger campaigns separately selectable.

Preserved reviewer tools are starting points for that deliverable. Prefer the
I6-era `adiff56.py` and `linki56.py` for two current-tool builds: they use the
normal executable names and omit the removed listing selector. Their inherited
comments still use some v1/v2 terminology. The earlier analysis set includes
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

Once available, run the 200-program check before local review of runtime
changes. Broaden the option combinations and failure cases for shared binding,
evaluation, layout or publication changes and for release cutovers. Until the
harness exists, identify the actual selected checks and their gaps. Longer
campaigns belong to risky changes or an observed failure; documentation-only
edits do not need them.

Existing checks to reuse:

- [Project checks](tests/projects/README.md) cover frozen ROMs and performance
  surveys; `tests/verify_fceux_projects.py` already compares real-source binary,
  diagnostics and analysis products across several modes.
- `tests/test_output_failures.py` and the IR allocation/ownership harnesses
  provide fault-injection controls. Keep test hooks out of production builds.
- The missing reusable component is the generated-program differential across
  analysis options and multi-unit linking. The project ROM gate alone does not
  provide it. A timing script's built-in threshold is not a substitute for the
  owner-approved performance policy.

### Integration plan and completion criteria

The harness is an active item in [TODO](TODO) and the next validation-tooling
task. The external backup is a preservation step; usable tests belong in the
repository. Implement this as separately reviewed tooling work, keeping the
2.0 release preparation independent of a new test-framework project.

1. **Land the bounded differential.** Adapt `adiff56.py`, `linki56.py` and their
   generator into `tests/differential/`, with the 200-program command and
   coverage report described above. Include small public fixtures, structural
   comparison, a reducer and reproducible failing-case bundles. Fold useful
   assignment and self-consistency controls into this suite. Remove hard-coded
   paths and audit normalizations and exclusions. Validate the harness using
   identical builds and deliberately changed output, diagnostics and artifacts.
2. **Make it reproducible and automatic.** Track a baseline source commit and
   build recipe; build that reference and the candidate with matched settings.
   Use an explicitly identified candidate until the actual 2.0.0 release commit
   is available. Cache reference builds by revision, compiler, flags and host;
   binaries on one developer's machine must not be required. After local review,
   add the bounded fixed-seed check to Linux and macOS PR CI, with coverage and
   failure bundles retained as artifacts. Keep larger campaigns manually
   selectable. Keep ordinary `make check` usable without a reference build.
3. **Integrate failure and malformed-input checks.** Adapt the allocation,
   publication, backtrace and object-mutation tools into the existing fault and
   object suites where practical. Track their synthetic fixtures. Prefer the
   existing portable test hooks; add platform-specific interposers only where
   necessary and validate each platform. Preserve the path/destination conditions
   needed to reproduce individual failures. Keep expensive sweeps opt-in.
4. **Retire dependence on the backup.** Record which maintained test or helper
   replaces each useful reviewer tool, and which older variants are superseded.
   Keep the original snapshot for provenance; normal development and CI must
   never read it. Mark the TODO complete only when a fresh checkout on supported
   Linux/macOS hosts can build the declared baseline and run the documented
   checks without private projects, review scratchpads or owner-local files.

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
