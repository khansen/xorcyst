# D1 performance protocol

Status: approved by the owner as part of D1 on 2026-10-23.
This defines how to apply D0's **no slower than
v1** requirement, not a relaxation of it. There is no v2 candidate or measured
v2 speedup yet. Single capture-build elapsed times are feasibility evidence,
not benchmark samples.

## Inputs and builds

Use the frozen baseline manifests in `design/v2/BASELINES.md` in the source checkout (not shipped). Build
v1.9.12 and the candidate from clean sources with the same compiler, flags,
linker, dependencies, architecture and optimization level. Hash executables;
record source revisions, compiler/OS versions and dynamic libraries. Do not
compare an installed old executable to a freshly built candidate. Benchmark
release builds without sanitizers or fault hooks; check correctness separately
under sanitizers. Candidate and baseline objects are built independently.

The mandatory object projects are dpadhero2, ntxdev, bombsweeper and all 12
music directories. Record Makefile target **and actual linker output path**:
some old Makefiles name a different target than their linker script's output.
Delete/rebuild products only inside scratch copies. Never reuse a v1 `.o`, a
previous ROM, or a generated asset from either timed run. Both versions consume
the same hashed, verified generated assets. Disable converter recipes in gate
builds (`make -o` for every frozen generated target), and use failing converter
stubs to catch missing exclusions. Check asset hashes before and after each build.
Time converters separately, outside the XORcyst gate; an unstable converter run
is not evidence about assembler/linker speed or correctness. In particular,
xm2nes's Wishmaster output depends on the environment and must not be regenerated
inside a gate build.

Use an explicit minimal environment, including a private HOME/TMPDIR, controlled
PATH, LC_ALL=C and TZ=UTC. Do not inherit shell variables, MAKEFLAGS, tool overrides,
DYLD_* or XASM_TEST_* hooks. Record the exact environment and command arrays.
Baseline verification uses two environments with different scratch/tool paths;
paired timings use matched environments for both versions.

Use frozen Metroid and Zelda for large pure-binary workloads. Metroid's exact
source hash is part of the gate; do not silently replace it when Nesrev evolves.
Add a new fixture revision explicitly if inputs change, with a fresh baseline.
No project or slow mode may be dropped because another mode improved.

## Configurations and useful work

For both large disassemblies, measure plain assembly; text listing; JSON and
NDJSON listing version 2; xref alone, data+owner, and embedded records; standalone
records+manifest; summary; index patterns; data consumers; coverage; comparison;
raw-address audit; and FCEUX NL. Include the supported text/CSV/NDJSON forms used
by actual wrappers, not only the default JSON forms. The invocation inventory
is frozen alongside the consumer scripts.

Measure all eight actual bundle profiles with the frozen bundle producer and
reader: ci-data-v1, ci-instructions-v1, inventory-instructions-v1,
process-instructions-v1, maturity-instructions-v1, instructions-v1,
data-listing-v1 and pass-prep-instructions-v1. Preserve warning policy, requested
facts and outputs. For the listing migration, compare v1's current useful
workflow to the adapted workflow, and also report the producer-only version-2
listing control. Neither schema migration nor cache hits may hide missing work.

For games/music, report separately:

1. Total object-mode xasm time for the complete source list, including process
   launches and each original option set.
2. xlnk time with that build's own complete object set and unchanged script.
3. Complete serial clean-build time from frozen assets. Report converter timings
   separately, outside this gate.

For analysis report producer-only time, downstream validation time and complete
bundle time separately. Baseline uses its frozen consumer; candidate uses the
reviewed migrated consumer. All three gates must pass. Record peak resident
memory and artifact sizes for every mode. Counts of emitted instructions,
records, references and fill extents prevent measuring reduced output instead
of equal useful work.

**Owner's memory limit, 2026-10-24:** some increase over matched v1 is acceptable,
but an increase of 30% or more is not. Require candidate peak RSS below 1.30
times baseline peak RSS for each required configuration; report paired samples
and each version's maximum observed peak as well as medians. I3 applies this to
plain Metroid and Zelda; adding objects and analysis does not waive the limit
or the no-slowdown rule for those later configurations.

## Sampling

Run on an otherwise idle machine with stable power/thermal settings. No parallel
builds, competing benchmarks, profiler, CI queue time or filesystem cache flush
between partners. Warm each command twice. Pre-create matched scratch copies
and output directories outside the timed region. Output writing, hashing that
the tool ordinarily performs, and process launch remain inside it.

Choose a fixed batch count from the slower warmup so each measured batch takes
at least two seconds. A batch invokes the normal command N times, using fresh
output destinations and the same frozen inputs. It is not a special in-process
benchmark path. Keep N identical for baseline and candidate. For a clean-build
sample, each repetition starts from its own clean isolated copy; source copying
is outside, all assembler/linker build work inside (converter generation is separate).

Take 15 serial paired batches, alternating AB then BA. Measure elapsed and
user+system CPU time for the complete process tree, not just the parent shell.
Record peak RSS using the platform's documented units. Keep raw samples and
stderr; any failed or mismatching run invalidates that measurement, rather
than becoming a fast sample. Do not discard slow samples merely as outliers.
If external interference is observed, annotate it and rerun the whole session.

## Decision rule

For each configuration and metric, use the paired log ratios candidate/baseline.
Report the median ratio and a paired-bootstrap 95% confidence interval for that
median (10,000 resamples; recorded deterministic seed). Pairing must be preserved
in resampling. Also report every sample, run order and batch size.

- **Pass:** the median candidate/baseline ratio is at most 1.000 for both
  elapsed and CPU time, all requested output/fact checks and measured stages pass,
  and no regression is confirmed in an independent session. A confidence interval
  crossing 1.000 does not prevent a pass when these conditions hold.
- **Suspected regression:** the lower endpoint is above 1.000 for either metric.
  Repeat that configuration in a fresh, independently warmed session on an idle
  machine, with new copies and a reversed initial AB/BA order. Use the same batch
  size and at least the same number of pairs; do not pool sessions to hide a failure.
- **Fail:** the lower endpoint is above 1.000 for the same configuration and
  metric in both sessions. This is the definition of a reproducible regression.
  Profiling can explain it but cannot waive it.
- **Unresolved:** a median exceeds 1.000 without a confirmed regression, or the
  two sessions disagree. Extend each affected session to 30, then 60 pairs.
  Require the pass conditions in each session used to decide that configuration.
  If they are still not met, return the evidence for review; do not label it a
  performance pass or cherry-pick a third favorable session.

The owner selected this equal-performance rule on 2026-10-22. It does not demand
a demonstrated speedup in every mode. With 100+ comparisons, a single confidence
interval can be a chance finding: report all comparisons, and require independent
confirmation before calling one a regression. There is no inherited 5%/20 ms
allowance, no averaging across workflows, and no speedup elsewhere to pay for a
regression. The complete protocol is included in the section 8 D1 approval
record in `XORCYST_V2_CONTRACTS.md`.

Instruction counts are explanatory only. Record memory/artifact regressions
even when time passes and review them; do not invent a numerical memory budget
without measuring the actual projects. Also measure a candidate-rich relaxation
cascade and shared aliases/scoped offsets to expose pathological growth. They
supplement, and never replace, the required real-project gate.

## Evidence retained

Keep machine/tool/input manifests, command arrays, exit codes, full raw timings,
output hashes, correctness-validation results and profiles with the reviewed
candidate revision. Project assets/ROMs remain in the owner's local archive;
commit the manifests and concise report. Repeat final combined-tree comparisons
after the consumer migration and before cutover. A green CI test suite alone is
not a performance pass.
