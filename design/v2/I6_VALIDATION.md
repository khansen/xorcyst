# I6 cutover validation

2026-10-26. Candidate for local independent review; not owner approval, a
published PR, or a release. XORcyst is based on I5 merge `6095465`; the paired
Nesrev tooling branch is based on public-master merge `30b8382e5`. Neither the
Nesrev projects branch nor project files are part of this change.

## What changed

Normal `xasm` and `xlnk` use the shared implementation, now named `ir/` with
`ir_*` private APIs. The alternate targets, engine switches, old object codec,
old layout/emission passes and listing-schema selector are removed. Version is
2.0.0; source and migration documentation explicitly call it unreleased.
Structured listings default to schema 2; instruction records remain schema 3.
Historical contracts, decisions and review evidence remain in `design/v2/`.

This is more than a target rename. Review the removed conditionals and tests
against `I6_TEST_MIGRATION.md`. Existing expression controls also exposed
captured aliases bypassing undeclared-name checks: missing scoped locals now
reject, and implicit external aliases warn at their use. The prototype's
ineffective linker verbose flag now reports file progress; the old verbose RAM
symbol-map dump is not retained. Both changes are described in the review
prompt and user documentation.

Nesrev's five shared producers drop `--listing-version=2`. Its docs require the
normal 2.0 tools and its CI pin is set to the local XORcyst commit after that
commit exists. Publish the producer first after approval. This avoids a cyclic
commit reference; the validation manifest hashes the eight paired tooling/docs
files, while the pin is visible directly in the paired commit.

## Build, package and tests

All final evidence below is under `/private/tmp/xorcyst-i6/`. The committed
`i6-validation.json` hashes the source inputs, tools and named evidence files;
`I6_ARTIFACT_SIZES.json` records every surveyed product's byte size. These are
maintainer records, not installed documentation or private project inputs.

| Check | Result | Evidence |
| --- | --- | --- |
| Source-archive `make distcheck` | 19 regression suites, 526 tests plus shell regressions; 5 installed-tool tests; archives ready; no skips | `distcheck-reviewed.log` |
| Installed interface | Only normal tool targets; 2.0.0 versions, docs/manpages, object/link/binary round trip; uninstall and distclean pass | Same log |
| Linux strict build | GCC 13.3, Ubuntu 24.04 aarch64; complete `-Werror` build; six IR modules also `-Wconversion -Wtype-limits` | `linux-final.log`, `linux-final.sh` |
| Linux ASan/UBSan/LSan | 19 value, 32 layout, 67 frontend, 38 object tests; unprivileged user; no reports or skips | `linux-final.log` |
| macOS ASan/UBSan | Final frontend suite: 67 tests, no reports | `macos-frontend-final.log` |
| HTML | All 12 files regenerate identically with Texinfo 7.3, no warnings | `html-final.log` |
| Wire example | Unchanged 244-byte example passes its checker | `wire.log` |
| Diff hygiene | `git diff --check` clean | Local commit preparation |

Four test docstrings and the value-suite class name changed after the successful
distcheck; no assertions or executable logic changed. Final documentation and
the archive were refreshed afterward. The final Linux source archive predates
only the backend test harness's corrected `ir/` include path; that harness
passes in the subsequent successful distcheck. Final runtime is covered by both
the Linux run and the final macOS frontend run.

Earlier exploratory failures remain in scratch for traceability. The first
directory name, `core/`, collided with Autoconf's crash-dump cleanup and left
`confdefs.h`; renaming it `ir/` fixed distclean. One AST instrumentation harness
still included the former path; correcting it fixed the next distcheck. Neither
failed run is claimed as final evidence.

## Required project and consumer gates

- All **15 frozen source-built ROMs are byte-exact**, including mandatory
  dpadhero2, bombsweeper, ntxdev and all 12 music projects. Builds use copies,
  explicit tools and frozen generated assets; converter traps and source hashes
  remain checked. See `roms-reviewed/results.json` and `roms-reviewed.log`.
- All **451 objects are byte-identical to approved I5**, and each project's
  stderr is identical. `object-identity.json` records the comparison and all 15
  object totals. No wire-format change was made.
- **52 Metroid/Zelda configurations** produce exact reference PRGs and the same
  analysis/listing/debugger products as v1 after time, output-path and producer
  identity normalization. The four raw comparison differences are two nested
  timestamps and the two manifests' executable identities. The normalizer
  verifies the actual producer bytes before excluding their size/hash/path.
  See `modes-reviewed/normalized-report.json`, `modes-normalized.log` and
  `normalize-modes.py`; raw `report.json` is retained.
- All **eight bundle profiles for each game** build and load through the paired
  Nesrev reader, with exact PRGs. See `bundles-reviewed/report.json`.
- Nesrev's full shell invocation passed **686 of 687 cases**. The remaining
  supervised-FCEUX test could not use `ps` inside the sandbox. Its unrestricted
  rerun passed all **20 Python tests**, and the separately run Java suite passed
  **1,206 tests**. Strict playbook checks report zero warnings. The initial
  `make test` invocation itself was not green; these are constituent checks,
  not a claim that the sandbox failure disappeared. Logs: `nesrev-test.log`,
  `nesrev-fceux-unrestricted.log`, `nesrev-java.log`, `nesrev-docs-final.log`.

The frozen ROM gate manifest still hashes
`5eaccd61551501e36d28d495ee1a8cf8fbb1daed81291d0a2382cd0f8dfb1e30`.
The frozen Metroid input still hashes
`9bb872dd7b0d619286230b8aade7828a3ac5ec67d3cb843f447bfd1edde873fb`.
Original project trees, frozen tools and system-installed tools were not edited.

## Bounded resource survey

Five alternating pairs on one macOS arm64 machine, with validated output on
every accepted sample. This is a bounded survey, not the full multi-session
statistical protocol. It follows the owner's I4/I5 decisions to document small
absolute costs and investigate material regressions. Ratios below are medians
of paired candidate/v1 ratios; time differences are differences of medians.
Raw samples and artifact sizes are in `performance/report.json`,
`performance-objects/results.json` and `performance-bundles/report.json`.

| Producer mode | Metroid elapsed ratio / difference | Zelda elapsed ratio / difference | Metroid / Zelda RSS ratio |
| --- | ---: | ---: | ---: |
| Plain | 0.890 / −7.58 ms | 0.831 / −8.54 ms | 0.850 / 0.833 |
| Text listing | 0.922 / −7.48 ms | 0.877 / −8.64 ms | 0.985 / 0.923 |
| JSON listing | 0.965 / −6.39 ms | 0.923 / −10.67 ms | 0.964 / 0.894 |
| Combined xref | 1.049 / +11.10 ms | 1.028 / +4.34 ms | 1.061 / 1.024 |
| Instruction records | 0.990 / −2.18 ms | 0.983 / −2.74 ms | 1.052 / 1.028 |
| FCEUX files | 0.987 / −1.37 ms | 0.885 / −8.77 ms | 1.088 / 1.048 |

Combined-xref CPU costs are +15.98 ms (Metroid) and +5.23 ms (Zelda). These are
reported costs, not a strict per-mode no-slowdown pass. Large-project producer
RSS stays below 1.30× in every sampled configuration.

| Object project / stage | v1 → candidate elapsed median | Paired elapsed ratio | v1 → candidate peak-RSS median |
| --- | ---: | ---: | ---: |
| dpadhero2 assembly commands | 353.31 → 234.15 ms | 0.664 | 8,830,976 → 6,078,464 bytes |
| dpadhero2 link | 9.36 → 8.87 ms | 0.955 | 2,572,288 → 3,293,184 bytes |
| dpadhero2 make build | 455.12 → 394.85 ms | 0.868 | 8,781,824 → 6,078,464 bytes |
| DPadHero3Proto assembly commands | 143.52 → 93.39 ms | 0.652 | 8,208,384 → 5,570,560 bytes |
| DPadHero3Proto link | 4.86 → 5.62 ms | 1.107 | 2,048,000 → 2,637,824 bytes |
| DPadHero3Proto make build | 98.68 → 79.91 ms | 0.803 | 8,159,232 → 5,521,408 bytes |

Assembly-command totals and make builds are separate measurements, not additive
rows: process startup and make invocation differ. The small linker memory excess
remains about 0.59–0.72 MB. The approved fixed-width tables, locations, graph,
translation workspace and temporarily live object buffer explain the cost;
the I4 accounting and custom-heap tradeoff remain applicable. Link CPU medians
are 8.12 → 7.63 ms and 3.81 → 4.47 ms respectively.

Complete bundle elapsed differences for `ci-data-v1` / `instructions-v1` are
−10.48 / −6.22 ms on Metroid and −7.57 / −1.86 ms on Zelda. Producer and consumer
stages are also measured separately: the largest observed extra elapsed cost is
18.72 ms for the Metroid instructions consumer. The largest paired RSS ratio
among the 12 bundle-stage configurations is 1.128. These small samples do not
establish a universal speedup or guarantee future workloads.

Object sizes are unchanged from I5: dpadhero2 totals **972,884 bytes** against
v1's 190,624, and DPadHero3Proto **389,128** against 40,968. The full 15-project
totals, all 52 product sets and final tool/archive sizes are recorded in
`I6_ARTIFACT_SIZES.json`. No compact-wire redesign is justified by this survey.

## Retained limitations and review gaps

`TODO` and the migration guide explicitly retain leading unnamed RAM `$`,
incomplete inherited parser/collector allocation diagnostics, rich-analysis
alias-tree growth, very long shared-label `sizeof` scans, duplicate size
diagnostics, provisional diagnostic extents, and the diagnostic-listing/FCEUX
planning limitation. Detected assembler publication failures roll back the set;
linker output publication remains per file. The known unused-label-warning
difference through constant aliases is also retained. This cutover does not
claim to resolve those separate issues.

No PR CI, real 32-bit/big-endian host, emulator run, new large differential fuzz
campaign or formal performance gate is claimed. The codec's conversion helper
is covered without a big-endian machine. Review and owner approval must precede
publishing the paired branches; green CI and coordinated merged heads must
precede tagging/releasing 2.0.0. No system install has been performed.
