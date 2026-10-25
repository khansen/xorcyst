# I4 local validation — independent review complete

Status: local independent review cleared `cf14315` with no blockers. The owner
approved `0805822` on 2026-10-25 for PR publication and CI; see the
[I4 approval record](../../v2/README.md#i4-approval-record). Earlier pending
status statements below are historical. The branch is based on merged I3 (`cb98568`). Installed
`xasm` and `xlnk` remain v1; the two v2 tools are non-installed check targets.
The owner approved the tag-8 clarification and accepted the current measured
memory tradeoff on 2026-10-25 and subsequently approved I4 as recorded above. The local reviewer
prompt is [I4_REVIEW.md](I4_REVIEW.md). The final sections supersede earlier
in-progress gate statements; older failures and measurements remain evidence.

## Implemented scope

- Owned XOROBJ2 writer, checked reader and independent byte-level codec tests.
- Source object output; exported values/aliases, implicit and explicit imports,
  deferred string/initializer extents and captured character maps.
- Multi-unit linking with v1 script grammar and approved strict numeric parsing,
  bank values, exact RAM sort and
  first-fit behavior, shared layout, final range checks and staged publication.
- One-object-at-a-time translation. Native values, bytes and spans outlive the
  temporary object storage; binding metadata is released before layout.
- Fixed-size slabs, compact expression/location records and compact indexed
  layout snapshots. Both query-mode caches and immutable string lifetimes stay
  intact. These shared representation changes also affect plain assembly and
  therefore require rerunning its gates.

There is no v1 object reader, installed-tool cutover or analysis renderer here.
The current linker uses a tested, link-owned arena/pool (`v2/heap.c`); scratch
prototypes with global state or incomplete cleanup were not adopted.

## Memory cost and design tradeoffs (2026-10-25)

The owner accepts the current measured I4 memory overhead for now. These are
mostly small NES programs; a small absolute cost can be reasonable when its
purpose is explained. This specifically accepts the current DPadHero3Proto
excess over the earlier below-30% limit; it does not claim the measurement is
below the limit or approve I4. Per-tool reporting remains required. The design,
contract section 12 and performance protocol record the decision.

Reviewers should assess both the cost and the complexity of the implementation,
including the existing custom heap, and look for simple, measured savings.
Do not add machinery merely to chase a percentage. This acceptance does not
excuse leaks, unexplained growth or unnecessary state, and does not waive
ROM equality, timing or the remaining validation/review gates.

Candidate-12's matched link session (`perf-links-12c.noindex`) measured
3,391,488 versus 2,670,592 bytes for dpadhero2 (+720,896 bytes, +27.0%).
DPadHero3Proto measured 2,736,128 versus 2,097,152 (+638,976 bytes, +30.5%);
its worst paired ratio was 1.315. The latter failed the then-applicable memory gate.
The session stopped after eight completed configurations to investigate that
failure and the suspected small-project timing regressions; it is not a full
15-project gate pass. Every completed sample preserved ROM bytes and diagnostics.

Separate, **instrumented allocation accounting**, not a timing/RSS benchmark,
records these requested v2 allocations after dpadhero2 layout:

| Storage | Bytes | Purpose |
| --- | ---: | --- |
| Layout fragments, auxiliary records and placement | 366,240 | One shared layout and final encoding, bank/RAM positions |
| Expression nodes, results, reverse edges and evaluation work | 361,216 | Deferred values, correct invalidation and repeated layout queries |
| Definitions and address anchors | 118,305 | Symbol identity, imports and final addresses |
| Source spans and file names | 100,911 | Located link-time diagnostics |
| Raw program data and copied files | 148,150 | Owned bytes independent of the temporary object reader |
| Remaining link/unit bookkeeping | 21,608 | Unit names, handles, directories and fixed state |
| **Total requested storage at this snapshot** | **1,116,430** | Includes allocated capacity |

The corresponding tracked v1 project allocations are 546,440 bytes after
layout (548,000 bytes at their allocation peak). V1 retains compact bytecode
and walks it directly; v2 expands it into layout records. V1's expression nodes
and root-pointer arrays occupy 215,544 bytes, versus 361,216 for v2's graph,
caches and invalidation state. V2's diagnostic spans are an additional service.
It allocates **zero bytes for a complete output buffer or full analysis fact
array** in this byte-only link. Listing/xref rendering is not causing this cost.

These are live requested bytes, not an exact partition of RSS: allocator size
classes, page rounding, executable/library pages, stacks and temporary peaks are
separate. V2's snapshot counts only its private heap, while v1's wrapper tracks
project malloc/calloc/realloc calls, including its script structures. The small
C-library/script allocations outside the v2 heap are not in its table. The
instrumented executables are not used to claim resource-gate results.

The core semantic requirements explain some extra state, but do not mandate
this exact representation. Array/slab spare capacity and construction-only
workspaces remain implementation costs, not new language features. Candidate-13
makes the translation walk stack grow with actual graph depth rather than
zero-initializing one slot for every node. It adds no new representation or
evaluation policy. More elaborate allocator/encoding work
needs a demonstrated benefit, not merely a smaller percentage.

Accounting sources and logs are in `profile13.noindex` (v2) and
`profile-v1-memory.noindex` (v1) under `/private/tmp/xorcyst-i4`. They read scratch
copies of the captured object builds. No source project or frozen asset changes.

## Correctness evidence

The reproducible source-checkout driver is `check_object_projects.py`. It uses
an explicit minimal environment, frozen generated assets and failing converter
stubs; all builds run in copies. Input hashes are checked before and after.

`/private/tmp/xorcyst-i4/gates-baseline-01/results.json` records separately built
v1.9.12 tools. Candidate captures `gates-candidate-02` through
`gates-candidate-11` each reproduce all **15 required ROMs**, including dpadhero2,
ntxdev, bombsweeper and all 12 music projects. Each capture assembles **451**
objects from source. `-03` includes streaming translation; `-05` includes the
compact records, decoder ownership transfer, coalesced boundary anchors and
linker path-comparison optimization. `-06` adds the assembler path fast path,
direct export traversal, 24-byte memos and streamed output. `-08` adds further representation changes: it also includes lazy macOS Unicode-framework loading, shared layout
checkpoints, reusable link translation workspace, 16-byte memos, 28-byte
fragments with 16-byte payloads, a denser location index and lazy decoded blob
metadata. Hashes and exact commands are in each capture.
Original project trees and the external frozen archive are untouched.

The object suite covers malformed envelopes and records, reserved tags,
reference/field bounds, fixup ownership, shared and cyclic graphs, a 20,000-node
DAG, imports/exports, bank and RAM placement, strings/maps, initializer kinds,
zero remainders, code limits, output aliases and I/O failures. A separate
allocation-injection executable sweeps each builder's allocation positions and
requires zero retained allocations after destruction. The transport driver
compares copying and ownership-transfer decoders on every valid/malformed input.

Candidate `-05` passed `make check` (516 tests, 19 suites plus shell
regressions) and `make distcheck` (521 tests, 20 suites, archives ready), with
no failures or skips. All four v2 suites passed macOS ASan/UBSan and Ubuntu
24.04 aarch64/GCC 13.3 ASan/UBSan/LeakSanitizer; all six new modules also passed
strict conversion/type-limit warnings. Logs are under `/private/tmp/xorcyst-i4/`.
A generated differential ran 300 multi-unit links against v1, including reversed
unit order, banks and RAM alignment: all bytes matched.

Since then, candidate `-06` passes the value (19), layout (31), frontend (45)
and object (23) suites. The new layout test compares streamed and buffered bytes,
full and compact facts, chunked wide/mapped strings, fill, branches, fragment
slices and snapshot stability after a graph edit. Full suites and sanitizers
must be refreshed for these later executable changes; earlier results are not
claimed for the streamed encoder or compact epoch.
The approved 244-byte wire example is unchanged. The fixture is in EXTRA_DIST
because the unpacked object suite reads it; private captures are not packaged.

## Performance blocker

The paired driver is `measure_object_projects.py`; it validates each measured
ROM and diagnostic stream and rechecks tools/input hashes. It records separate
object assembly, linking and serial full-build configurations. Link samples
reuse immutable objects in a private scratch copy and remove the product before
every invocation; every resulting ROM is checked before removal. Assembly/build
repetitions start in fresh scratch copies. Setup, validation and cleanup are
outside the summed per-invocation time intervals. Object hashes are checked as
well as source/asset hashes. Converters never run in these gates.

The first formal dpadhero2 linker session, against candidate capture `-04`, is
`/private/tmp/xorcyst-i4/perf-link-dpadhero2-01/results.json`: 15 alternating
pairs, 293 ordinary linker processes per batch. All ROMs match. Results:

| Metric | candidate / v1 | 95% paired bootstrap interval |
| --- | ---: | --- |
| elapsed median | 1.3395 | 1.3143–1.3591 |
| CPU median | 1.4022 | 1.3731–1.4150 |
| maximum RSS | 2.2795 | v1 2,637,824 bytes; candidate 6,012,928 bytes |

This session **fails** both resource gates. It predates the 40-byte expression
records, decoder ownership transfer and path-comparison optimization. Exploratory
runs show improvement but are not replacement gate results. Retain this failed
session; the later acceptance covers the current measured memory cost, not
this superseded candidate's higher cost or any timing regression.

The next formal session, `perf-dpadhero2-02/results.json` against `-05`, has
15 alternating pairs for each stage:

| Stage | elapsed median | CPU median | maximum RSS ratio |
| --- | ---: | ---: | ---: |
| link | 0.9008 | 0.8796 | 1.9938 |
| complete build | 1.1157 | 1.1393 | 1.2263 |

Link time passes in that session, but linker memory and build time do not.
`perf-dpadhero2-03` measured candidate `-06` before lazy framework loading and
the later record changes. Link elapsed/CPU medians were 0.9174/0.8999, but
maximum RSS was 4,947,968 versus 2,637,824 bytes (1.876 times v1). Assembly
medians were 1.0179/1.0309 and complete-build medians 1.0197/1.0177, so those
time gates did not pass either. Retain these results; they do not describe the
current runtime.

`perf-plain-01/results.json`, after the assembler path/export changes but before
the streamed encoder and compact memo epoch, reproduces both reference PRGs and
v1 stderr across 15 alternating pairs:

| Project | elapsed median | CPU median | maximum RSS ratio |
| --- | ---: | ---: | ---: |
| Metroid | 0.9252 | 0.9250 | 1.0135 |
| Zelda | 0.8512 | 0.8508 | 0.9760 |

Both pass in this session. The final tree needs its own confirmation. At this
point the standalone linker memory gate remained open regardless of complete-build
peak; the subsequent current-cost acceptance is recorded above.

Still required: current matched timing sessions (including independent
confirmation), current Metroid/Zelda plain byte/diagnostic/timing/RSS gates,
remaining object project resource configurations, final sanitizer/full-suite
checks, local independent review and owner approval. I5 analysis/consumer work
has not started. Do not publish this branch while I4's gates remain open.

## 2026-10-25 per-tool follow-up

The owner initially retained the separate-tool RSS gate: a complete build
below 1.30 times v1 cannot waive a standalone linker failure. PERFORMANCE.md
records that decision and the later acceptance of the current measured cost.
Scratch allocator prototypes guided the subsequent
link-owned implementation below; none of their exploratory measurements is a
formal gate pass.

Before the heap follow-up, targeted checks passed: values 19, layout 31,
frontend 46 and objects 23.
The compact-fact control now pins addresses and output offsets across bank-end
padding and an origin reset, in both full and indexed fact modes. Decoder
mutation controls exercise first-edit blob materialization, appending borrowed
bytes, adding a borrowed blob and allocation failure during those operations.
`layout-final07-gcc.log` records the strict GCC 16 layout run; the earlier
`layout-compact2-gcc.log` failed on a bitfield-conversion warning, corrected by
masking the already validated width. Candidate `-08` contains that correction.

`check-final08-unrestricted.log` records 518 tests in 19 suites plus shell
regressions. The first sandboxed run stopped at the existing Unix-socket test;
it is not a passing full-suite run. The unrestricted rerun passed with no skips.
macOS ASan/UBSan passes values and objects (`*-final07-asan.log`) and layout
(`layout-compact2-asan.log`). Linux GCC 13.3/aarch64 ASan/UBSan/LeakSanitizer
passes all four v2 suites; all six v2 modules pass strict conversion/type-limit
checks (`linux-final08/`). The later explicit checkpoint assertions were run
separately in `layout-checkpoint-controls.log`; that test-only extension is not
in the Linux snapshot. The 244-byte wire example still passes unchanged.

`perf-dpadhero2-04/results.json` measures candidate `-08` and fails:
link elapsed/CPU medians 0.9392/0.9272, but RSS 4,456,448 / 2,637,824 = 1.6894.
Assembly medians are 1.1498/1.1809 and RSS 1.5912; full-build medians are
1.1173/1.1184 and RSS 1.6011. Fresh output names still triggered framework
loading; earlier overwrite-output probes did not expose that. Candidate `-09`
adds the ASCII-name comparison, retaining Unicode normalization and fail-closed
framework handling. All 15 ROMs remain exact.

Candidate `-11` introduced the link-owned heap. It adds cleared traversal marks
(32-byte nodes and 20-byte anchors), shared provider leaves, interned link
metadata spans, and the private linker heap. The assembler continues using the
C allocator. All 15 ROMs and diagnostic streams match, from 451 object builds.
`marks-differential/{before,after}.log` compares 5,878,292 mixed queries with
38,646 span nodes and 340,145 rollovers: zero differences, matching digest
`77b978c921d38aed`. This isolates the traversal-mark change before heap adoption.

The value (19), layout (31) and object/heap (24) suites pass locally; the
object/heap suite also passes macOS ASan/UBSan. All seven isolated modules pass
GCC 16 with `-Wconversion -Wtype-limits -Werror`. Attempting those flags on the
entire frontend exposed existing v1 conversion warnings as well as one new
anchor conversion warning; the latter is fixed with a cast after validation.
The allocator tests exercise size classes, stable records, growth/shrink,
independent heaps, zero/overflow cases, slot reuse and failure of every mapping,
including pooled decoder/link integration. Linux GCC 13.3/aarch64 ASan/UBSan/LeakSanitizer now passes all four suites
(19/31/46/24), and all seven modules pass strict conversion/type-limit checks
(`linux-heap11/`). Formal resource measurements for this head remain in progress.
`Makefile.in` was regenerated with Automake 1.19; trailing whitespace on the
three newly generated rule/variable lines is normalized for diff hygiene.
Full distcheck and a current plain-project resource session remain to be rerun.


## Candidate-11 dpadhero2 sessions

`perf-dpadhero2-05/results.json` measures candidate `-11`, 15 serial alternating
pairs in each stage, on matched builds. Every measured ROM and diagnostic stream
matches. No competing builds/tests ran during sampling.

| Stage | elapsed median | CPU median | max RSS v2/v1 |
| --- | ---: | ---: | ---: |
| object assembly | 0.6797 | 0.7059 | 0.7790 |
| standalone link | 0.9632 | 0.9494 | 1.2795 |
| complete build | 0.8638 | 0.8474 | 0.7829 |

Standalone linker peaks are 3,375,104 and 2,637,824 bytes. This session passes
each tool's separate memory and time requirements; it does not waive any other
project/configuration. Fresh independent confirmation, the remaining fixtures,
current plain-project measurements and final full checks are still outstanding.


The independent `perf-dpadhero2-06` session uses the same batch counts and a
reversed initial order. Its completed linker stage keeps RSS below 1.30x
(1.2638 maximum ratio, 1.2795 worst pair), but elapsed/CPU medians are
1.0028/1.0013, with lower confidence bounds above 1.000. Because session `-05`
was faster, the candidate-11 linker time result is **unresolved**, not a pass
or a confirmed regression. The in-progress output batching change targets
per-fragment stdio calls; it needs fresh correctness and performance evidence.

`make check` and `make distcheck` pass candidate-11: 519/524 tests in 19/20
suites plus shell regressions, no skips, archives ready. macOS ASan/UBSan also
passes the test-only strengthened heap teardown assertions; the Linux snapshot
predates those additional assertions, but contains the same runtime.


Candidate `-12` adds output batching: small fragments share a 4 KiB staging
buffer, large runs write directly, and the final partial buffer is always
written before returning success. Buffered and streamed bytes still match;
write-failure injection covers each flush, including the final partial flush.
All 15 ROMs/451 objects and diagnostic streams remain exact (`capture12.log`).
The layout suite (31) and object suite (24, macOS ASan/UBSan) pass. A single
explanatory instruction-count comparison drops from 83,848,555 to 78,821,227;
it is not a formal timing gate. Both ROMs are identical. Current resource
measurements must be taken against `gates-candidate-12`.


The first all-project timing attempt (`perf-all-12`) stopped during baseline
Bombsweeper warmup: adding `-o` did not override the script's explicit
`output{}` command, so the harness checked an empty file. Both linkers already
match v1's output-switch semantics; a new control pins it. Link timings now use
fresh copies and the original scripts/arguments, just like assembly/build timings.
Candidate-11 dpadhero2 measurements are unaffected: its script has no explicit
output command. Other exploratory preflight figures using that override are not
gate evidence.

The corrected `perf-links-12b` session was stopped after one pair because of
observed background indexing (mds 192%, mds_stores 138% CPU). Raw data and
`INVALID.txt` are retained. No timing conclusion is drawn from that session.
Subsequent copies use a `.noindex` directory to avoid triggering indexing.


Current candidate-12 validation: Linux GCC 13.3/aarch64 passes values (19),
layout (31), frontend (46), and objects (25) with ASan/UBSan/LeakSanitizer;
all seven v2 modules pass strict conversion/type-limit checks (`linux-buffer12`).
The 300 generated multi-unit links again match v1 byte-for-byte, including
reversed unit order, banks and RAM alignment (`random-links-buffer12.noindex`).
The full check passes 520 tests in 19 suites plus shell regressions
(`check-buffer12.log`).


## Candidate-13/14 follow-up

Candidate-13 shares the assembler's lazy Unicode name comparison with the linker,
so case/canonically equivalent new output names cannot publish over each other.
The native-volume test covers distinct names on Linux as well as aliases on macOS.
It also sizes the translation stack by graph depth; forward-ordered and reversed
20,000-node shared graphs, and injected failures during stack growth, pin this.
The object suite (26) passes plain and macOS ASan/UBSan. All 15 ROMs remain exact.

Candidate-14 avoids rewriting already-native little-endian object table words.
The explicit conversion loop remains for other host byte orders; no big-endian
host was available. The object suite (26) and all 15 ROM captures pass.
`perf-focus-14.noindex` measures Bombsweeper, dpadhero2 and DPadHero3Proto only;
it is not the full gate. Current source/tool hashes refer to candidate-14.
The memory accounting builds also reproduce both reference ROMs;
`/private/tmp/xorcyst-i4/memory-accounting.json` records their hashes.

Candidate-14 also passes the object suite (26) under macOS ASan/UBSan, the
frontend suite (46), and all seven isolated modules under GCC 16 with
`-Wconversion -Wtype-limits -Werror`. These do not replace a final Linux or
full check/distcheck run for this head.

The focused candidate-14 session completes 15 pairs per project:

| Standalone linker | elapsed median | CPU median | maximum RSS ratio | worst paired RSS |
| --- | ---: | ---: | ---: | ---: |
| Bombsweeper | 1.0439 | 1.0679 | 1.1667 | 1.1667 |
| dpadhero2 | 0.9850 | 0.9789 | 1.2857 | 1.2857 |
| DPadHero3Proto | 1.1458 | 1.1852 | 1.3071 | 1.3071 |

DPadHero3Proto peaks at 2,719,744 versus 2,080,768 bytes: 638,976 bytes extra.
This exceeds the original memory limit and is now explicitly accepted for the
current implementation, subject to the cost/complexity review described above.
Bombsweeper and DPadHero3Proto exceed the original timing ratios. Their absolute
cost and the subsequent owner guidance are recorded below. No completed formal
resource gate is claimed for all 15 projects.

## Final validation and bounded timing work (2026-10-25)

The owner asked to continue with bounded effort and explicitly deprioritized
single-digit-millisecond slowdowns; hundreds or thousands of milliseconds
warrant investigation. Contract section 13 and PERFORMANCE.md record this
guidance. The existing candidate-14 formal link samples, normalized by batch
count, give these per-invocation values:

| Standalone link | v1 elapsed median (ms) | v2 elapsed median (ms) | Paired elapsed delta (ms) | Paired CPU delta (ms) |
| --- | ---: | ---: | ---: | ---: |
| Bombsweeper | 2.676 | 2.798 | +0.118 | +0.119 |
| dpadhero2 | 8.454 | 8.302 | -0.126 | -0.157 |
| DPadHero3Proto | 4.286 | 4.897 | +0.619 | +0.601 |

`absolute-focus-14.json` derives these values from the preserved formal samples;
paired deltas are medians of within-pair differences, not differences of medians.
No additional runtime optimization was made to chase these sub-millisecond
costs. The short `--survey` mode reports five alternating pairs, one invocation
per sample, across object assembly, link and complete build. It checks every
ROM and the input hashes, reports absolute times and ratios, and is explicitly
not a formal no-slowdown pass or confidence-interval claim.

Current-runtime validation is now complete for these checks:

- `make check`: 521 tests, 19 suites plus shell regressions, no skips.
- `make distcheck`: 526 tests, 20 suites, no skips; archives ready.
- Linux GCC 13.3/aarch64: values 19, layout 31, frontend 46 and objects 26,
  with ASan/UBSan/LeakSanitizer; no reports. All seven isolated modules also
  pass strict `-Wconversion -Wtype-limits -Werror` compilation.
- The unchanged 244-byte wire example passes its checker.

Logs are `check-final14-unrestricted.log`, `distcheck-final14.log` and
`linux-final14/*.log` under `/private/tmp/xorcyst-i4`. The restricted first
`make check` attempt failed because the sandbox blocked binding a Unix socket;
the complete unrestricted rerun passed, including that test. The release
archive is retained outside the worktree as `xorcyst-1.9.12-final14.tar.gz`.
The Linux source snapshot SHA-256 is
`b6184d658eeb62b8814aaaf75aafd4b1ed0b54d725f9af6eb5d1d970f92e8b8d`.
The follow-up changes only documentation/metadata and the source-checkout
timing harness. Runtime and tool hashes still match candidate-14.

### Bounded object-project survey

`survey-final14.noindex/results.json` records five alternating pairs per stage,
with one invocation per sample, for all 15 projects (45 configurations).
All ROMs and diagnostic streams match and input/object hashes are unchanged.
This is descriptive evidence for practical cost, not a formal ratio pass.
The table reports median paired elapsed deltas in milliseconds per complete
stage; object assembly includes all project objects, not just one invocation.

| Project | Assembly delta (ms) | Link delta (ms) | Clean-build delta (ms) |
| --- | ---: | ---: | ---: |
| bombsweeper | -5.442 | -0.062 | -2.242 |
| dpadhero2 | -123.376 | -0.282 | -59.539 |
| music-bob-omb-battlefield | -39.682 | +0.311 | -16.272 |
| music-dire-dire-docks | -39.280 | +0.562 | -15.923 |
| music-double-dragon-ii-vs-f1 | -39.196 | -0.062 | -15.168 |
| music-dpadhero3proto | -43.522 | +0.219 | -20.237 |
| music-is-it-scary | -38.804 | +0.375 | -17.103 |
| music-live-and-learn | -32.829 | +0.042 | -14.161 |
| music-mega-jean-trooper | -34.777 | +0.427 | -13.238 |
| music-merry-xmas-everybody | -37.828 | +0.048 | -16.327 |
| music-somewhere-only-we-know | -35.777 | +0.337 | -15.277 |
| music-there-must-be-an-angel | -36.161 | -0.046 | -12.750 |
| music-welcome-to-paradise | -35.473 | +0.442 | -15.218 |
| music-wishmaster | -45.613 | +0.249 | -16.516 |
| ntxdev | -107.019 | -2.308 | -44.788 |

All assembly and clean-build elapsed/CPU medians improve; the largest positive
link delta is +0.562 ms elapsed and +0.516 ms CPU. No surveyed stage shows a
hundreds-of-milliseconds increase. Per the owner's guidance, no further
percentage-only optimization cycle is required for these small link costs.

Per-tool memory remains reported, including variation between sessions. In this
short survey dpadhero2 links at 3,391,488 versus 2,588,672 bytes (1.310 maximum
ratio, 1.327 worst pair); DPadHero3Proto at 2,703,360 versus 2,080,768 bytes
(1.299 maximum ratio, 1.320 worst pair). These are the same small absolute-cost
range already accepted for the current implementation, not strict below-30%
passes. The raw `passes` field evaluates the original limit only. All other
surveyed link maximum ratios are below 1.30; complete-build peaks are lower
than v1 for all 15 projects. Keep per-tool results visible rather than using
those build improvements to obscure the linker's retained storage.

### Final plain-project session

`perf-plain-final14.noindex/results.json` records 15 serial alternating pairs
with matched tools and the frozen Metroid/Zelda inputs. Every PRG and diagnostic
stream matches v1. The results are candidate/v1 ratios:

| Project | Elapsed median | CPU median | Maximum RSS ratio | v1 / v2 peak bytes |
| --- | ---: | ---: | ---: | ---: |
| metroid | 0.9186 | 0.9346 | 0.8919 | 48,480,256 / 43,237,376 |
| zelda | 0.8304 | 0.8461 | 0.8539 | 40,271,872 / 34,390,016 |

Both elapsed/CPU confidence intervals are wholly below 1.0 and both memory
ratios are below the original cap. This is one formal session; no independent
confirmation is claimed. The bounded object survey and accepted small-command
costs remain separate from these large-project results.

I4 is ready for local independent review. The final manifest records source,
tool and evidence hashes; no runtime changes were made during this final
validation follow-up. No I4 implementation approval, PR or push is claimed.
I5 listing/xref/bundle validation is still future work, and no actual 32-bit or
big-endian host was tested. The reviewer should assess correctness, cost and
complexity under the explicitly recorded owner choices.

## Independent I4 review and focused follow-up

The local reviewer examined `6d91699` and found B1 (empty layout diagnostics),
C1 (undocumented strict numeric parsing), and optional items O1–O8. The owner
selected strict parsing on 2026-10-25. Review clearance and owner implementation
approval remain pending for this follow-up; no PR or push has occurred.

### Diagnostics, coverage and bounded optimization

- B1: link diagnostics now fall back to value/layout errors before final layout,
  including their locations. Missing/unreadable `copy` inputs have a script/path
  message; origins above $FFFF have a located range error. Tests preserve seeded
  outputs and exercise every heap mapping failure in a multi-unit link. Input
  table allocation failures also keep their out-of-memory message.
- C1: the contract ledger, NEWS and README record the approved full-token numeric
  parsing. Tests reject the four reported spellings plus invalid binary prefixes,
  and retain valid decimal/hexadecimal/binary forms.
- O3: bank-result lookup uses binary search over the already sorted node IDs,
  without adding another index. A short three-sample 16K/32K-use probe measured
  reviewed I4 at 50.8/159.6 ms and the fix at 21.2/31.4 ms, with byte-identical
  objects (`review-bank-scaling.noindex`). This is explanatory scaling evidence,
  not a project timing gate. Interleaved and repeated bank targets have a source
  regression test. The later leaf sharing can change object bytes, but not these
  bank semantics.
- O4: duplicate-export and undefined-import diagnostics name the symbol; borrowed
  names live with the link until destruction. Tests pin both messages.
- O5: the production word-conversion loop is a helper exercised directly by the
  codec test, with non-symmetric words and both separate and aliased buffers.
  This covers the loop on little-endian CI, not an actual big-endian host.
- O6: a binary-mode test pins forward mapped `HI` in a two-byte char field to
  `01 00 07`, without the stale padding byte, both plain and debug.
- O7: documentation now states that pooled blocks have no ASan quarantine and
  LeakSanitizer cannot see individual suballocations. Live allocation/byte checks,
  default-libc coverage and failure injection complement sanitizers; clean runs
  are not proof of pooled lifetime safety. No quarantine machinery was added.
- O8: the top-level README now distinguishes pending re-review from unfinished
  resource measurement.

The three new/strengthened diagnostic tests fail four assertions with the
reviewed linker runtime restored (`review-parentproof.log`), with no harness
errors. The pre-leaf follow-up passed 526/531 check/distcheck tests; the final
writer revision's results below supersede those counts.

### Peak memory: reviewer trace versus retained allocations (O1)

The earlier allocation table is a post-layout snapshot and does not explain
peak RSS by itself. The reviewer instrumented `6d91699` with getrusage and
current RSS from task_info. The peak occurred during largest-object translation:

| Project | Before game.o | After read | Translation peak | After raw buffer freed | After layout |
| --- | ---: | ---: | ---: | ---: | ---: |
| DPadHero3Proto | 2,228,224 | 2,490,368 | 2,719,744 | 2,441,216 | 2,375,680 |
| dpadhero2 | 2,916,352 | 3,162,112 | 3,375,104 | 3,063,808 | 3,211,264 |

At that peak the raw game.o buffer (268,816/299,316 bytes respectively) and
roughly 40 KB translation workspace overlap the growing native representation.
The reviewer estimated this transient accounts for at most about 278 KB of
DPadHero3Proto's excess and 164 KB of dpadhero2's. Layout and writing did not
raise the peak. These are reviewer measurements of the reviewed runtime, not
new measurements of the slimmer writer below.

The reviewer found no peak-RSS gain from mmap input. Replacing the custom heap
with malloc raised dpadhero2 from 3,391,488 to 4,046,848 bytes and DPadHero3Proto
from 2,719,744 to 3,047,424, with similar speed. This is evidence for retaining
the heap, not proof that all packing/capacity choices are necessary. Their
scratch evidence is under `scratchpad/i4.1VN84Z/var/` in the review archive.

### Object sizes and conservative writer change (O2)

No requirement demands a 6–11x artifact expansion. Fixed-width u32 tables buy
simple checked decoding and native table access on little-endian hosts, but the
original writer also stored redundant spans and duplicate leaves. The reviewer
estimated roughly 3x wire overhead from fixed widths versus variable integers
and delta lines; their experimental dpadhero2 representation was about 383 KB.
That was a scratch estimate, not a production codec or approved format change.
A compact wire alone would still need expansion or a new decoding/translation
strategy to reduce peak RAM. We are not adding that machinery for disk savings.

The actual writer now shares definition-reference and address leaves by owner
ID and omits their ignored expression spans. It retains literal, operator,
bank-expression, RAM-position wrapper, definition/anchor and fixup locations.
Two small construction-only ID arrays replace repeated equivalent records; no
iterative graph-normalization pass or wire change is needed. Cycle proxies use
the owner location when a reference lacks an expression span. Tests check
record sharing, retained spans, exact linked bytes and located cycle errors.

I5 uses direct-binary facts. Contract section 6 rejects object-mode structured
listings/xref and introduces no xlnk analysis consumer, so this does not remove
required I5 per-term provenance. Any future object-analysis proposal must define
its own metadata requirements rather than assume unused leaf spans are a promise.

The figures below are actual source-built artifacts, not the reviewer's
post-processing experiment. `review-object-sizes.json` records the reviewed
objects and table-byte counts; `review-leaf-sizes.json` records this writer.
All 451 objects rebuild and all 15 ROMs/stderr still match the frozen baseline.

| Project | v1 bytes | Reviewed I4 bytes | Current bytes | Reduction | Current / v1 |
| --- | ---: | ---: | ---: | ---: | ---: |
| bombsweeper | 12,832 | 113,388 | 97,380 | 14.1% | 7.59x |
| dpadhero2 | 190,624 | 1,116,264 | 972,884 | 12.8% | 5.10x |
| music-bob-omb-battlefield | 34,333 | 262,688 | 224,444 | 14.6% | 6.54x |
| music-dire-dire-docks | 36,659 | 352,436 | 295,080 | 16.3% | 8.05x |
| music-double-dragon-ii-vs-f1 | 21,561 | 204,520 | 174,236 | 14.8% | 8.08x |
| music-dpadhero3proto | 40,968 | 466,764 | 389,128 | 16.6% | 9.50x |
| music-is-it-scary | 33,908 | 274,616 | 235,044 | 14.4% | 6.93x |
| music-live-and-learn | 32,088 | 265,964 | 226,924 | 14.7% | 7.07x |
| music-mega-jean-trooper | 32,949 | 207,456 | 181,940 | 12.3% | 5.52x |
| music-merry-xmas-everybody | 26,047 | 211,272 | 180,988 | 14.3% | 6.95x |
| music-somewhere-only-we-know | 17,199 | 170,388 | 145,180 | 14.8% | 8.44x |
| music-there-must-be-an-angel | 26,436 | 250,720 | 211,572 | 15.6% | 8.00x |
| music-welcome-to-paradise | 25,063 | 186,340 | 161,132 | 13.5% | 6.43x |
| music-wishmaster | 39,057 | 249,008 | 217,752 | 12.6% | 5.58x |
| ntxdev | 62,765 | 655,412 | 554,928 | 15.3% | 8.84x |

This is the conservative variant that keeps literal spans. It saves 12.8% for
dpadhero2 and 16.6% for DPadHero3Proto, close to the reviewer's 13%/17% estimate.
The remaining 5.1x/9.5x size is an explicit implementation tradeoff, not a language
requirement or a claim of optimal encoding. The reviewer attributed roughly
30–35% of original object bytes to locations, 20–23% to expressions, 19% to
fragments and 12% to fixups. Future optimization should address demonstrated
cost, not reopen the format solely to improve a ratio.

### Final follow-up validation

- Object suite: 32 tests pass plain and macOS ASan/UBSan.
- Frontend suite: 47 tests pass plain and macOS ASan/UBSan (before the object-only
  leaf change); the final full and Linux runs include the current writer.
- `make check`: 528 tests, 19 suites plus shell regressions; no skips.
- `make distcheck`: 533 tests, 20 suites; no skips, archives ready.
- Linux GCC 13.3/aarch64: values 19, layout 31, frontend 47 and objects 32 pass
  with ASan/UBSan/LeakSanitizer; all seven modules pass strict conversion/type-limit
  compilation. No reports.
- All 15 ROMs and diagnostic streams match in `gates-review-leaves.noindex`.

Logs are `review-leaves-*.log`, `linux-review-leaves/*.log`, and the earlier
`review-fix-*.log` under `/private/tmp/xorcyst-i4`. No real 32-bit or big-endian
host or PR CI result is claimed. The large historical timing sessions remain
attributed to their original runtime; this follow-up uses bounded measurements.

### Bounded follow-up cost survey

Five serial alternating pairs, one invocation per sample, cover the two projects
from the object-size review. Every output is checked. These short samples describe
absolute costs; they do not establish sub-millisecond equality or isolate the
leaf optimization from other follow-up changes. In particular, a paired median
delta need not equal the difference between the two separate medians.

| Project | Stage | v1 median ms | v2 median ms | Paired median delta ms | v1 max RSS bytes | v2 max RSS bytes |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| dpadhero2 | assembly | 357.77 | 239.70 | -117.79 | 8,863,744 | 6,242,304 |
| dpadhero2 | link | 9.12 | 8.83 | -0.32 | 2,637,824 | 3,325,952 |
| dpadhero2 | build | 460.84 | 404.88 | -57.60 | 8,814,592 | 6,176,768 |
| music-dpadhero3proto | assembly | 142.08 | 93.92 | -46.91 | 8,241,152 | 5,586,944 |
| music-dpadhero3proto | link | 5.15 | 5.51 | +0.09 | 2,048,000 | 2,654,208 |
| music-dpadhero3proto | build | 99.68 | 81.84 | -18.48 | 8,192,000 | 5,652,480 |

`survey-review-leaves.noindex/results.json` records the raw commands, tools,
objects and samples. Current link peaks are about 1.261x/1.296x v1 respectively;
the comparison with older sessions is not a controlled measure of the leaf
change's RSS saving. Assembly and whole-build peaks are lower than v1. There is
no material cost here that calls for another tuning cycle under the owner's
bounded-effort guidance.

A separate one-run correctness check of plain Metroid and Zelda gives identical
PRGs and stderr to v1 and the frozen reference; see
`review-leaves-plain.noindex/results.json`. The earlier formal plain timing
session belongs to `6d91699`, not this follow-up. No further formal timing claim
is made for the current revision.

## Second I4 review: labels and reassignment

The local review of `42ff0c0` cleared the previous fixes but found B1 (object
lowering after unused-label pruning) and B2 (stale captures after assigning a
value derived from its previous deferred definition). B1 dates to I4; B2 also
affects merged I3 direct-binary output. Both are fixed in this I4 follow-up.
No new semantic decision is introduced: D0 already requires exported aliases
to resolve like local equivalents, and D1 requires capture before later `=`.
This revision remains subject to local independent review and owner approval.

### Changes and regression controls

- Reassignment advances the capture generation **after** reducing the RHS and
  replacing the definition. Previously, RHS capture of the old `S` could be
  cached in the generation intended for the new `S`. Forward local and imported
  values now give `05 06 0C`; address captures give `04 80 05 80`. Tests also
  cover an alias retaining an earlier capture and deferred strings. The binary
  controls run with debug disabled and enabled.
- Export graphs are captured after second-pass public tagging and before
  unused-label pruning. Capturing their dependencies marks the otherwise-unused
  private labels that must survive. This supports direct, arithmetic, low-byte,
  difference and chained aliases, declared before or after their code, plus a
  RAM-label export. It does not retain all unused labels or change ordinary RAM
  allocation order. The existing D0 alias requirement authorizes support here;
  no new owner decision or language feature is required.
- Storage lowering omits a PC anchor for object-mode RAM counts that do not use
  `$`. V1's rule that leading unnamed RAM storage has no allocation identity is
  then reachable for `.dsb`, including after one or more labels are pruned.
  Tests retain the unused-label warnings and `AD 00 02` bytes. A surviving-label
  control consumes its full four-byte span and fails when the RAM block is a
  byte too small; a genuine PC-dependent count retains its anchor requirement.
- The frontend diagnostic adapter substitutes `location unavailable` when no
  file exists, avoiding a NULL `%s` argument. A writer failure checks that text,
  exit status 1, previous object preservation and absence of temporary files.
  This does not claim every writer error now has a source span.

Only `astproc.c` and `v2/frontend.c` change executable code in this follow-up.
The `astproc.c` changes are guarded by `XORCYST_V2_FRONTEND`; preprocessing the
v1 variant with assertions disabled is byte-identical to `42ff0c0`
(`review2-v1-preprocess.json`). Normal assertion line constants can move.
No linker, value/layout evaluator, wire writer, allocator, format or build
wiring changes are involved.

The five new test methods fail **33 assertions** on a fresh export of `42ff0c0`
with only those test files and the generated build configuration copied in:
10 frontend and 23 object assertions, with no test setup or harness errors.
Logs are `review2-parentproof-{frontend,objects}.log`. This parent proof covers
all reported failure paths without an additional broad mutation campaign.

### Current verification

- Frontend: 48 tests pass plain and macOS ASan/UBSan.
- Objects: 36 tests pass plain and macOS ASan/UBSan.
- Full check: 533 tests in 19 suites, plus shell regressions; no skips.
- Distcheck: 538 tests in 20 suites; no skips, archives ready.
- Linux GCC 13.3/aarch64: values 19, layout 31, frontend 48 and objects 36 pass
  with ASan/UBSan/LeakSanitizer. All seven isolated modules compile with strict
  conversion/type-limit warnings. No reports.
- All 15 required ROMs rebuild exactly from 451 source-built objects, and stderr
  matches `42ff0c0` for every project. Frozen source/asset hashes are rechecked.
- All 15 aggregate object sizes are unchanged. In particular, dpadhero2 remains
  972,884 bytes and DPadHero3Proto 389,128 bytes. The prior size reductions hold.
- Plain Metroid and Zelda match the reference PRGs and v1 stderr in one current
  correctness run each. No new formal timing or RSS session is claimed; prior
  measurements remain attributed to their reviewed runtime.
- The 244-byte wire example is unchanged and its checker passes.

Evidence is under `/private/tmp/xorcyst-i4`: `review2-*.log`,
`gates-review2.noindex/results.json`, `review2-object-sizes.json`,
`review2-plain.noindex/results.json` and `linux-review2/`. The initial sandboxed
check was blocked by the socket-publication test (`Operation not permitted`);
`review2-check-unrestricted.log` is the complete passing rerun. The initial
object-test draft had invalid test syntax; the final logs and parent proof use
valid explicit import annotations and a nonempty empty-output unit.

The manifest separates generated `config.h` from source hashes. Configure may
legitimately change that file in a reviewer's scratch build; its hash records
this build's configuration rather than a portable source identity. Tool hashes
and current evidence hashes still identify the binaries actually tested.

The optional unreadable-link-input diagnostic still names the object path at
line 0 rather than the script line; it is unchanged. No 32-bit or big-endian
host, new project timing gate or PR CI result is claimed. Nothing was pushed.

## Third I4 review: warning counts and RAM compatibility

The review of `02264e0` confirmed its label-pruning and reassignment fixes, but
found B1: early exports were counted as source uses for unused-equate warnings.
It also reported C1/C2, two older RAM compatibility differences. This revision
fixes B1 and follows v1 for both RAM cases, consistent with D0's existing
placement policy. It introduces no owner-approved exception or stricter policy.
Independent re-review and I4 owner approval remain pending; nothing is pushed.

### Final implementation

- When W0201 checking is enabled, early export capture snapshots constant
  reference counts and restores them after dependency capture finishes. Labels
  retain the references needed for exported aliases. Private constant chains
  keep the source-use counts that `warn_unused_equates` expects. The temporary
  list/counts are freed on success and allocation failure; ordinary builds
  without this option allocate no snapshot. The symbol table membership does
  not change during export capture.
- With no RAM blocks, layout still calculates/checks RAM sizes, then skips
  placement before the first allocation. Anchors remain unresolved, so local
  and imported address uses fail. The check is not repeated after allocation
  consumes blocks: supplying too little RAM still fails normally. Extent
  validation, sorting with blocks and first-fit behavior remain unchanged.
- Leading unnamed RAM storage is discarded even when its count uses `$`, as
  in v1. A count is not serialized if there is no allocation identity for it.
  Storage following a surviving label still contributes to its allocation.
  RAM initializer values remain separate and still require evaluation.

A preliminary local version skipped RAM extent validation too. A v1 probe
showed that `calc_data_addresses` precedes `map_data_to_ram`'s no-block return,
so that version was withdrawn. The final change is the four-line early return
**after** `ram_size`, without another layout flag or retained state. The final
negative control requires an unresolved RAM-label count to fail without blocks.
Preliminary `review3-*` logs are not the final runtime evidence; current runs
use `review3-final-*` and `linux-review3-final/`.

### Controls and evidence

The object suite checks exact W0201 text and locations for literal exports,
private dependencies, forward definitions and chains, plus a genuinely used
control. It runs in both object and binary mode, with warnings disabled,
`--warn-unused-equ` and `--Werror=unused-equ`. Promoted warnings must exit 1,
keep previous output and leave no temporary file.

RAM tests cover unused and unused-exported variables with no blocks, known and
forward constant counts, required local/imported addresses, unresolved counts,
exhausted supplied blocks, and leading `$+1` storage with/without removed labels
in debug and ordinary object mode. The existing surviving-label size control
continues to require all of its storage. The no-location writer-error test now
uses a required RAM initializer address rather than pinning the withdrawn
leading-storage rejection.

Three new/extended controls fail **23 assertions** against a fresh `02264e0`
export with current tests, with no harness errors. See
`review3-final-parentproof.log`. Direct frozen-v1 comparisons in
`review3-v1-controls.json` verify six RAM success/error cases and twelve warning
configurations. For forward exports v1 can additionally reject serialization;
only their W0201 lines are compared, since D0 already approves v2's export fix.
The reproducer is `/private/tmp/xorcyst-i4/review3-oracles.py`.

### Final validation

- `make check`: 535 tests in 19 suites plus shell regressions; no skips.
- `make distcheck`: 540 tests in 20 suites; no skips, archives ready.
- macOS ASan/UBSan: objects 38, layout 31 and frontend 48 pass, with no reports.
- Linux GCC 13.3/aarch64: values 19, layout 31, frontend 48 and objects 38 pass
  under ASan/UBSan/LeakSanitizer; all seven modules pass strict conversion and
  type-limit compilation. No reports.
- All 15 ROMs and stderr match the reviewed head. All 451 source-built objects
  are byte-identical, so every recorded artifact size remains unchanged.
- Plain Metroid and Zelda match reference bytes and v1 stderr.
- The 244-byte wire example is unchanged and its checker passes.
- Production v1 sources and build wiring are unchanged. No new timing/RSS run,
  32-bit/big-endian host or PR CI result is claimed.

Current evidence lives under `/private/tmp/xorcyst-i4`: the final logs above,
`gates-review3-final.noindex/results.json`, `review3-final-object-sizes.json`,
`review3-final-plain.noindex/results.json`, and the current manifest's hashed
paths. Prior measurement sessions remain attributed to their earlier runtimes.


## Independent clearance of cf14315

The owner's local independent reviewer reported no blockers at `cf14315`.
This records review completion, not owner approval or authorization to publish.
The follow-up recording this result changes documentation only; the runtime,
tests, build wiring, source/tool hashes and wire example remain unchanged.

The reviewer independently confirmed exact W0201 text across eight sources
with both warning and Werror modes, including private dependencies, forward
chains, reassignment and real source uses. Promoted warnings preserve output.
Unused RAM without blocks and leading PC-dependent storage now match v1;
required addresses, unresolved sizes and insufficient supplied blocks reject.

Reported validation at `cf14315`:

- Values/layout/frontend/object suites: 19/31/48/38, plain and macOS
  ASan/UBSan, with no reports.
- All 15 ROMs and stderr match; all 451 objects are byte-identical to `02264e0`.
- Two linker differential seeds: 319/319 and 335/335 v1-accepted cases
  produce identical bytes.
- Deferred reassignment: 1,000 binary and 500 imported-object cases match v1.
- All 33 injected linker allocation failures report out of memory, preserve
  output and leave zero live mappings; the next run produces the exact ROM.
- Plain Metroid/Zelda and the source, evidence and tool hashes match.

The reviewer did not rerun full check/distcheck, Linux builds, linker fuzzing
or RSS timing. Those remain attributed to the earlier runs, not this review.
No tests were rerun solely for these documentation edits.

### Optional leading-RAM initializer limitation

`$` in leading unnamed RAM data still fails object writing. For example,
`.dataseg / x: .db $ / v .byte / .codeseg / lda v` links to `AD 00 02` in v1
and works in v2 direct-binary mode, but v2 object mode reports an unplaced
anchor without a useful source location. Bare `.dataseg / .db $` also fails.
Data after a surviving RAM label works. Other RAM initializer symbols must
still resolve, so blindly dropping all initializer evaluation is inappropriate.

This predates the latest fixes. The reviewer classified it as optional and
non-blocking. It is tracked in TODO for a focused compatibility fix or a
separately approved limitation with a located diagnostic. Neither this review
nor this record approves a new semantic exception; no runtime change is made
in the review-status follow-up.

Reviewer evidence remains under
`/private/tmp/claude-501/-Users-kenthansen-dev-github-xorcyst/0ef1b090-a72f-41b3-b907-3b7d795327cb/scratchpad/i4.1VN84Z/`:
`c4/`, `roms4.noindex/`, `inject4/`, `work/injdph4/`,
`work/{weq,ramnone,ld3r,ld4}`, and the round-four scripts.
