# I3 local validation

Status: independent local review cleared runtime at `7ffb2ac` and the
test/documentation follow-up at `dc45658` with no findings or blockers.
Kent Hansen approved `dc45658181a755175658bb60ce3c1b38da1dfec2` on 2026-10-24
for publication as the single I3 PR and CI. See `v2/README.md` for the approval
record. The owner explicitly approved rejecting
self-sized strings on 2026-10-24; the contract ledger and NEWS now record that
separate compatibility decision. B1–B4, N1, S1/R1, F1, U1, A1, L1 and the preceding
memory changes were cleared by the prior independent reviews. Parent: `2083c7a768bc3ad141dc4eb70a1ed96f21a15f11`
(reviewed I2, PR #98). CI and the later implementation/cutover gates remain open.

## Coverage follow-up to the cleared review

The new `.exitm` control covers a macro ending after its label, multiple removed
body statements and a conditional exit, with debug off/on. The removed body
must emit no bytes, and its tombstones must not split the leading labels' size
scan. Expected outputs are `02 02`, `03 00 03` and `02 02`.

Runtime, headers, build wiring, contracts, NEWS, manual, HTML, CI and the
performance manifest are unchanged from `7ffb2ac`. Its full-suite, sanitizer,
project and performance results below remain applicable; they are not claimed
as new runs. The 45-method frontend suite passes plain and under macOS
ASan/UBSan. Removing only the tombstone skip makes all six new subtests fail
by assertion, with no harness errors. All 67 source hashes and the prototype
binary hash still match the unchanged performance manifest. `git diff --check`
is clean. Logs and the mutant are in `/private/tmp/xorcyst-i3-exitm/`.
Full check/distcheck, Linux sanitizers, project builds and performance were
not repeated for this test/documentation-only change.

The review's optional long-label-run cost and duplicate-diagnostic observations
are recorded in TODO. Neither is changed by this coverage follow-up.

## Prototype scope

The non-installed `xasm-v2` target lowers existing source syntax into the owned
value/layout modules and produces plain pure-binary output. Every invocation
destroys the parser AST and symbol table before layout and emission. Objects,
listings, analysis, debugger exports, comparison and manifests remain disabled.

## Review corrections

- Declarations retain their own identity across `.undef`. Retired labels,
  variables and procedures no longer place the replacement's anchor, and
  aliases containing address expressions resolve by the final-name rules.
  Tests include the D1 Nesrev `pair_redefined_label` fixture, both segments,
  arithmetic aliases, replacement by a constant, and debug builds.
- AST clones copy only the explicit-address rejection flag. Nested macro
  parameters keep v1's local-label binding, while `.label name = address`
  remains rejected inside macros and repetitions.
- Known byte/word RAM operands keep their located truncation warnings,
  including forward constants and mixed literal/fixup data. v1's silent late
  label-value and numeric RAM `.char` cases stay silent.
- The branch +128 source error preserves the previous output and its line-1
  location; +127 succeeds. Tests also pin deferred data/storage rejection of
  source-time PC use. The owner confirmed the PC and multi-string `.char`
  corrections in the migration ledger on 2026-10-24.

The N1 follow-up closes a remaining final-name kind assumption. A local label
can become a string after `.undef`; data that used it must not silently keep a
one-element `sizeof` or a provisional source-time PC. Nonliteral operands in
label-size scans now retain kind guards even when they currently name labels.
Source-time PC uses select the preceding numeric-width assumptions to check
after final-name binding. Those checks run before second-pass rewriting and
release all borrowed operands then. Separate segment prefixes and absolute
origin resets are preserved, and resets cannot discard an earlier PC use's
check. No check is needed when data has no source-time size/PC consumer.

The N1 controls cover direct names, arithmetic and aliases, empty/single/
multiple-character strings in size scans, all integer data widths, source-time
conditionals/captures/origins/repetition/messages, macro/procedure expansion,
segment switches and absolute origins. Stable numeric aliases and final numeric
or label replacements remain accepted. A string already known at the sizeof
use is measured at its real length.

The S1 follow-up extends each guard to the kind and string length counted by
that scan. It checks the emitted data-line binding rather than capturing the
transient definition visible at sizeof.
Final reassignment from string to integer, integer to string, or to a different
string length gives the located size-unknown error. The owner approved this
contract clarification on 2026-10-24. Stable numbers, same-length strings and
already-captured literal values stay accepted. Tests include all four reported
reproductions, empty strings, all data widths, debug builds and procedure scope.

R1 uses the ordinary first-pass local-label renaming before judging data widths.
Backward `@@` references, arithmetic over them and macro-local references now
permit a following source-time PC. The existing nested-macro binding controls
still pass. Additional tests cover multiple recorded operands before one PC use,
the validity of the whole prefix, and independent code/RAM prefixes across an
absolute code origin.

F1 requires the guard to use the operand's data-line capture, including when
sizeof scans an operand later in its own statement. U1 corrects F1's unsafe
assumption that every scanned statement survives pass 1: a user-typed data
statement is replaced and freed while being flattened.

No size guard retains an AST statement pointer. Native statements receive
handles into an owned table of pending guard lists, whether or not their data
callback has run. A live-tree walk after the first pass clones each flagged
statement's operands into its guards. The walk runs only if a native statement
has pending guards, and each statement visits only its own pending list.
The existing AST clone rule drops the pending flag, so cloned statements cannot
inherit a handle. The table and guards are freed on success and error paths.

A1 corrects the remaining capture timing: the data callback can return before
anonymous `+`/`-` references are renamed, and even a previously processed data
statement can still contain a forward reference awaiting its label. Waiting
until pass 1 ends preserves the data-line binding and sees those final names.
The earlier immediate-capture path and completed-capture marker are removed.
No anonymous-reference resolution or source binding rules are changed.

A guard on an unprocessed user-typed initializer cannot bind. At validation it
reports size unknown at the original sizeof use, preserving `190b881`'s clean
rejection. It never reads the replaced initializer. A typed instance processed
before the scan remains supported. This is a conservative limitation, not a
claim that forward sizeof scans through typed initializers are implemented.

The same-line forward `@@` sizeof case remains fixed: end-of-pass capture
sees normal first-pass renaming. The per-operand renaming order is unchanged.
Forward local names before source-time PC still use the conservative width rule.
All S1 kind/length and F1 capture controls remain in the source suite. New tests
cover `.tag`/`.S`, byte and mixed-width fields, forward values, intervening data,
plain/macro/procedure/rept contexts and debug builds. They assert the source or
macro-call location and previous-output preservation. Positive controls cover
instances before the use, independent native pending lists, several guards for
one operand, previously processed data, and repeated macro expansion.
The A1 controls add forward/backward anonymous operands before, after and on
the sizeof line, repeated markers, macro/procedure/rept contexts and debug
builds. Repeated macros, rept and while loops check independent expansion scopes.

L1 fixes repeated sizeof operands within one scan. An operand containing
sizeof of the scanned declaration contributes one element without recursively
reducing that size. Each use is evaluated normally when its own data operand
is processed. This applies to the scan's own operand too; its complete result
keeps the numeric-kind guard, since a concatenation with sizeof can be a string.
Such strings give the located size-unknown error instead of publishing a stale
size. Storage counts and type dimensions still use the existing cycle checks.
No statement pointer or guard lifetime changes are involved.

The two numeric reproductions produce `02 02` and `03 01 03`, matching v1.
The known-string reproduction produces `04 41 42 04`: the approved known-at-use
string rule counts S's two bytes at both scans, rather than retaining v1's
first `03`. Tests cover byte/word/dword widths, arithmetic around repeated uses,
separate operands and statements, labels ending a scan, other label sizes,
macro/procedure/rept contexts, debug builds and repeated macro expansion.
Negative controls preserve real storage/type cycles, unknown counts/types,
division errors and output preservation. Concatenations are checked both in
the use's own operand and in a later operand.

L1b extends that recognition to leading labels sharing the same data run.
The helper compares their first non-label/non-internal-marker statement at
the current scan, using the same boundaries as the scan itself. This directly
recognizes the shared extent without recursively reducing each label's size;
unrelated labels remain separate. Only temporary pointers within the call are
used. The pending-guard representation, lifetime and binding walk are unchanged.
Tests cover two/four labels, both declaration/operand orders, all data widths,
macro/procedure/rept scopes, internal markers and debug builds. The unrelated
label-string control emits `01 02 03 32 41`, while real storage cycles and
surviving directive boundaries still reject.

The owner decision is distinct from S1: `Tbl: .db sizeof(Tbl)+"A"` is rejected
even without reassignment or undef, rather than keeping v1's stale `31 41`.
Dedicated controls pin all four reviewed string forms and their original error
locations/output preservation. Numeric forms remain accepted. Duplicate size
diagnostics from multiple uses guarding one operand are unchanged; this
follow-up does not coalesce diagnostics.

## Memory and representation

The owner requires peak memory growth **below 30%** relative to matched v1,
while retaining the no-slowdown rule. The earlier 69.5/53.3 MiB candidate is
superseded. The follow-up avoids retaining two complete representations:

- Fully literal data is compacted after source-time size scans. Mixed operands
  keep individual fixups, and `.char` retains its segment-specific diagnostics.
- Lowered instruction/data/storage operands are freed as lowering proceeds;
  declaration nodes stay alive through namespace teardown.
- Immutable instruction integers live directly in operand fixups. Their bytes,
  ranges, narrowing, warnings and facts match integer-expression fixups.
- Anchor inspection no longer allocates throwaway expression nodes.
- The prototype AST stores its diagnostic location once (136 rather than 168
  bytes on arm64). I5 must capture original analysis provenance separately
  before enabling renderers. There is no claim that those modes work today.

Value nodes remain 288 bytes, with the two reviewed memos unchanged. Layout
fragments grow from 136 to 144 bytes to store the literal-operand discriminator.
Earlier I3 optimizations remain: pooled fixed widths, fixed prefix stretches,
extent-root reachability and capacity reservation. These representation edits
are in review scope; the prior I2 review alone does not cover them.

## Checks at `7ffb2ac`

- Strict Apple clang `-O2 -g -Werror` build of production tools and prototype.
- `make check`: 490 tests in 18 suites, plus shell regressions.
- `make distcheck`: 495 tests in 19 suites, archives ready, no skips.
- Source suite: 44 methods, passing under macOS ASan/UBSan and Ubuntu 24.04
  aarch64 GCC 13.3 ASan/UBSan/LeakSanitizer. Sanitized Linux Metroid and Zelda
  match v1 bytes and diagnostics, with no reports. The U1 cases are included
  in both sanitizer suites and the full check/distribution runs.
- Value/layout runtime and tests are unchanged from `5e8d059`: the prior
  18/29-method sanitizer and strict-conversion results still apply, and both
  suites pass again in the full check/distribution runs.
- L1b tests against `d4ffada` fail 146 assertions. Four mutations fail by
  assertion: stopping at a leading label (145), matching any label (1), stopping
  at an internal marker (1), and omitting self-size kind guards (8). No build
  or harness failures.
- Earlier L1 tests against `1dac72e` failed 98 assertions. Four mutations failed by
  assertion: recursively reducing self-size operands (98), checking only the
  operand root for sizeof (48), omitting their numeric-kind guards (6), and
  counting them as bytes for every directive (64). No build or test harness
  failures in the completed runs.
- Earlier A1 tests against `068efea` failed 68 assertions. Five mutations failed by
  assertion: skipping the end-of-pass walk (76), capturing native operands
  early (60), ignoring unbound guards (97), binding only the first guard (1),
  and using the wrong pending list (1). No harness or build failures.
- Earlier U1 tests against `c152a86` failed 97 assertions, catching the release crashes.
  Five mutations failed by assertion: ignoring unbound guards (97), skipping
  statement-local capture (138), not marking processed native data (1), binding
  only the first guard (1), and using the wrong pending list (1). No harness or
  build failures in these proofs.
- A typed-data generator checked 600 programs both plain and under macOS
  ASan/UBSan: 300 aggregate-before-use cases matched independently computed
  bytes, and 300 aggregate-after-use cases rejected with a size error and kept
  the prior output. It varies primitive field widths, values, directive syntax,
  intervening native data and expansion scopes. No sanitizer reports.
- A shared-label generator checked 600 programs against independently
  calculated bytes and both v1 binary and linked-object output, with no
  differences. The same 600 passed under macOS ASan/UBSan. It varies two to six
  labels, mixed data widths, operand counts, literal strings, arithmetic around
  sizeof, statement positions and macro/procedure/rept contexts (seed 2403).
- The earlier repeated-sizeof generator checked 600 programs against independently
  calculated bytes and both v1 binary and linked-object output, with no
  differences. The same 600 passed under macOS ASan/UBSan. It varies mixed
  data widths, operand counts, literal strings, arithmetic around sizeof,
  statement positions and macro/procedure/rept contexts (seed 2402).
- The earlier anonymous-reference generator checked 600 programs against independently
  calculated bytes and both v1 binary and linked-object output, with no
  differences. The same 600 passed under macOS ASan/UBSan. It varies marker
  direction/length, data widths, operand counts, sizeof position, preceding
  code, and macro/procedure/rept contexts (seed 2401).
- Earlier F1 controls against `190b881` gave 137 assertion failures. Three mutations
  failed by assertion: capturing before data-line reduction (137), discarding
  guards for the sizeof use's own statement (6), and capturing the first slot
  instead of the recorded operand (55). No build/harness failures. An initial
  mutation showed that whole-statement early renaming was unnecessary after
  deferred guard capture, so that additional runtime edit was removed.
- Earlier S1/R1 tests against `7a9fec3` gave 69 assertion failures. Eight mutants failed
  by assertion: omitting known-at-use guards (57), guarding the reduced scan
  copy (57), ignoring string length (24), late local renaming (12), ignoring
  earlier prefix validity (1), marking only the last operand (1), selecting the
  code prefix for a data PC (1), and resetting both segments (1).
- A self-checking 6,000-program workload (reviewer's reassignment generator,
  seed 11) accepted 4,217 programs and checked 2,077 PC captures and 6,948 label
  sizes, with zero inconsistent accepted outputs. One size rejection was
  accepted by v1 binary: a forward string reassigned to an integer after its
  sizeof use, covered by the approved S1 rule. This workload does not prove
  equivalence for every rejected program.
- Earlier N1 tests against `5e8d059` gave 81 assertion failures. Seven mutants failed
  by assertion: trusting current labels in sizeof (18), skipping PC validation
  (63), skipping width capture (63), missing absolute-origin reset (1), sharing
  segment prefixes (1), forgetting prior PC uses on reset (1), and dropping
  the pending-label lookup (1). No harness failures.
- Earlier tests against `d9ae457` gave 28 assertion failures. Seven targeted
  mutants are caught: live-name anchor reuse (20), early address capture (4),
  all clone flags (4), no clone flags (4), skipped mixed RAM warning (1),
  treating forward data as fixed width (4), and dropping storage PC deferral (4).
  There are no harness/build failures in these proofs.
- The direct-IR test compares literal and expression operands across twelve
  opcode/mode families, fourteen boundary values, both layout policies,
  forced width and early truncation. It compares bytes, diagnostics and facts.
  Anchor tests cover unknown/known placement, extent prefixes, invalid IDs and
  invalid locations. Existing cache and allocation-injection tests still pass.
- Existing expression/initializer source comparison: 108 methods, 18 differing
  runs across four unique sources, all approved type-body `$` rejections.
  This is not an object/analysis comparison or a complete project corpus.
- The manual, all 12 HTML files and wire example are unchanged from reviewed
  `d4ffada`; its regeneration/wire proofs still apply. The contract and NEWS
  record the newly approved self-sized-string decision. `git diff --check` is clean.

Production executables hash identically to the reviewed `d4ffada` build.
Production `xlnk` disassembly is identical to matched v1.9.12. Production `xasm`
differs only in 29 assertion source-line immediates. Neither production target
links the v2 modules. Parser, scanner, configure, man pages and version files
are unchanged.

## Plain-project performance

`i3-performance.json` retains the final raw samples, source/tool hashes,
compiler details, commands and minimal environment. The baseline is a fresh
v1.9.12 archive build with the same Apple clang 21 compiler and `-O2 -g -Werror`.
The runner uses frozen inputs, two warmups, batches over two seconds and fifteen
alternating pairs. Every output and diagnostic stream must match; both PRGs are
131,072 bytes. Converters do not run. Memory includes the entire process peak.

| Plain build | Elapsed ratio (95% CI) | CPU ratio (95% CI) | Maximum RSS, v1 → v2 | Growth |
| --- | --- | --- | --- | --- |
| Metroid | 0.9123 (0.9098–0.9142) | 0.9075 (0.9071–0.9116) | 46.1 → 53.0 MiB | +14.9% |
| Zelda | 0.8479 (0.8469–0.8504) | 0.8427 (0.8412–0.8454) | 38.3 → 41.0 MiB | +7.1% |

Both timing medians pass. Every paired peak ratio and the maximum observed
peak ratio are below 1.30. The independent reviews of the earlier heads confirmed their own
timing/memory gates. Those sessions remain separate; no earlier samples are
pooled with this follow-up.

## Limits and evidence

Validation covers macOS arm64 and Ubuntu aarch64 locally. No PR or CI run is
claimed. Real 32-bit/big-endian hosts remain untested. The full shell suite was
not instrumented. Generic v2 diagnostics still differ from some v1 wording;
located errors and output preservation are tested. An undefined name after a
source-time PC use can still be reported as a width error rather than by name.
A forward alias to a string can still produce size unknown where the direct
known-at-sizeof form succeeds; this pre-existing conservative rejection is not
changed by the guard fix. The earlier I3 prefix edits
can move cycle-error locations toward the responsible extent; identical old
cycle-line selection is not claimed.

Object/link ROM gates (including mandatory dpadhero2), analysis/listing
performance and downstream Nesrev migration remain I4/I5 obligations. Plain
speed and memory results waive none of them. Owner-local logs, mutants,
comparisons and final sessions are under `/private/tmp/xorcyst-i3-l1b/`; L1 evidence remains in
`/private/tmp/xorcyst-i3-l1/`; A1 evidence remains in
`/private/tmp/xorcyst-i3-a1/`; U1 evidence remains in
`/private/tmp/xorcyst-i3-u1/`; F1 evidence remains in
`/private/tmp/xorcyst-i3-f1/`; S1 evidence remains in
`/private/tmp/xorcyst-i3-s1/`; N1 evidence remains in
`/private/tmp/xorcyst-i3-n1/`; earlier
follow-up evidence remains in
`/private/tmp/xorcyst-i3-followup/` and original I3 evidence in
`/private/tmp/xorcyst-i3-evidence/`. Frozen inputs were only read. This report and timing manifest are not distributed.
