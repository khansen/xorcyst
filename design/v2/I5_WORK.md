# I5 analysis and consumer integration

Status: independent local review cleared `63e4ea5` / Nesrev `4ca95e06d`
with no blockers. The owner approved I5 on 2026-10-25 at `fe500f6` / `4ca95e06d`
for publication and CI. The consumer publication branch starts from public
master; project-specific changes and projects-branch history stay local. No
cutover is claimed; see `v2/README.md` for the approval record.
Base: I4 merge `63747edf648aed016928c4e2671c7f4ad4236c37` (PR #100), whose PR
and post-merge CI passed. Installed `xasm`/`xlnk` still use the existing engine.

## Producer implementation

All pure-binary products in the approved inventory are connected: text and
JSON/NDJSON listing, text/CSV/JSON xref, embedded and standalone instruction
records, summary, index patterns, data consumers, coverage, comparison,
raw-address audit, dependency manifests and FCEUX ROM/RAM exports. The single
structured listing is schema 2; xref stays at 2 and instruction records at 3.
Object-mode structured analysis is rejected as approved. I6 removes the
prototype names, old engine and selectable legacy schemas at coordinated cutover.

The frontend owns a source-row index whose entries identify ranges of layout
fragments. Native instruction/data/storage metadata transfers out of the source
tree after lowering; marker metadata and symbol definitions have independent
owners. Source paths are interned in the value unit, including original macro
spans and expansion spans. Instruction terms bind to the shared value graph.
The original source namespace and remaining parser tree are destroyed before
layout on every successful invocation. The retained AST-shaped metadata is a
read model for the existing collectors, not an alternative evaluator or layout
engine. No collector dereferences a parser symbol entry after teardown.

Final addresses, opcodes, modes, offsets, lengths and bytes come from the shared
layout facts. Integer expression and term queries use its value graph. String
metadata uses final encoded bytes, including captured character maps and widened
elements. Snapshot enumeration preserves v1's ordering. Symbol identities and
analysis provenance have separate fields from frontend cache IDs.

Rich original source spans add 32 bytes to a node's allocation only when needed
by instruction records or debugger provenance; the fixed node remains 136 bytes
on this host. Source metadata is retained only for requested products. Native
statements transfer ownership instead of being cloned; PC anchors are created
only when the actual operand or a recorded term needs `$`.

Text listings use the lightweight source-row index plus the symbol snapshot;
they do not retain or expand collector expression trees. Diagnostic rows also
have independent provisional lengths/opcodes, so layout errors cannot erase
JSON-only listings. Syntax-error paths retain the recovered parser statements.

The existing destination planner still precedes publication. All generated
files are staged until serialization, late value queries and dependency capture
have finished. Commit keeps rollback links for existing destinations; a failed
rename restores earlier destinations. The manifest is staged last. Stdout and
process/filesystem crashes are outside this rollback guarantee. Successful output
requires complete facts. The safe diagnostic-listing exception carries
`binary_generated: false`, no emitted bytes and null offsets; it cannot certify
a bundle or bypass NL/CSV/manifest destination checks. Unknown diagnostic
extents fail closed when those extents are needed to reserve destinations.
Collectors and provenance are destroyed before their shared value/file owner.

A probe also exposed silent loss of references when an expression could not be
represented in the existing bounded analysis expression view. That path now
reports a located error and preserves previous outputs. The 32/64/65-level alias boundary now has controls: graph reference indirections
do not halve the operator-depth allowance. Plain assembly is unaffected.
This is not a new evaluator limit or permission to silently omit facts.

## Nesrev migration

The consumer branch is isolated at `/private/tmp/nesrev-xorcyst-v2-analysis`,
based on `abc8ae3e1`; the owner's original checkout and project inputs remain
untouched. Frozen acceptance sources/consumers remain the D1 capture.

- Bundle validation requires successful listing-v2 rows. Non-emitting offsets
  are null, emitted ranges retain inclusive ends, and fills carry explicit
  byte counts. Diagnostic and malformed rows are refused.
- Named pointer-table checks and the embedded-pointer advisory get label
  positions from xref definitions, joined to listing source identity. Labels
  no longer acquire an invented zero offset.
- Static analysis uses `accumulator` for shifts/rotates, as the opcode-based
  listing specifies. The existing carry-confusion test detects a missed migration.
- Trace setup byte checks skip non-emitting rows and expand compact fills.
  Synthetic checks exercise those paths without launching an emulator.
- Current producers request listing version 2 explicitly. Profile names and
  bundle schema stay unchanged; stale cached bundles must be regenerated.
  Dated committed audit evidence remains historical, rather than being silently
  rewritten as evidence from a new producer.

## Validation so far

Evidence is under `/private/tmp/xorcyst-i5/`; source/build identities and the
final check list will be frozen in the validation record before review.

- All 15 required source-built ROMs match, using 451 newly assembled objects.
  Metroid and Zelda PRGs and diagnostics match their frozen v1 references.
- The full Metroid/Zelda product comparison matches v1 after timestamp/output
  path normalization. Manifests differ only in actual producer identity and
  arguments. All eight bundle profiles validate for both projects with the
  migrated consumer and exact PRGs.
- The 116-test analysis suite reuses instruction-record, symbol, access,
  debugger-export and publication/failure contracts. Object-mode negative cases
  assert the approved refusal rather than skipping them. macOS ASan/UBSan passes.
- The frontend suite now has 59 tests, including mapped forward field strings,
  assignment generations, shared-PC facts and deep-expression failure closure.
- Ninety focused Nesrev shell checks cover bundle/record validation, pointer
  audits, RAM/raw-address consumers, branch literals, split pointers, used-by,
  static analysis and orphan opcode scanning. One initial failure exposed the
  accumulator-mode migration and passes after that fix. A separate trace-setup
  test covers three consumers with good and corrupted byte/fill cases.
- Full check/distcheck pass 663/668 tests in 20/21 suites, plus shell regressions.
  Linux ASan/UBSan/LeakSanitizer passes the value/layout/frontend/analysis/object
  suites. Three restricted-directory subcases skip under container root; macOS
  and a separate unprivileged Linux run exercise them. The source-capture and
  consumer mutations fail their intended assertions. Earlier checkpoint logs
  remain historical.
- Nesrev repository hygiene passes. Its strict playbook check reports ten
  pre-existing stale references in historical review documents; the changed
  tooling guide stays within its route budget.

## Performance assessment

`measure_analysis.py` records paired elapsed/CPU/RSS samples, exact PRGs and
artifact sizes for individual products and every bundle profile. Its default
five-pair run is explicitly a bounded survey, not a formal no-slowdown pass.
The formal D1 sampling schedule remains available with `--pairs 15
--batch-seconds 2`. CPU accounting uses `wait4` rather than the two-decimal
printed CPU times of macOS `time`; the first coarse survey is retained only as
exploratory evidence.

The initial survey found small producer regressions, up to about 15 ms on
Metroid. A complete bundle initially added about 143 ms; eliminating repeated
successful-path error formatting and per-byte regex calls in the consumer
removed that cost in the first repeated profiles without weakening validation.
On 2026-10-25 the owner explicitly chose to document small I5 costs for local
review and focus further effort on material regressions. Contract section 14
records that decision. It is not a formal no-slowdown pass or I5 approval.
The 100-configuration survey reports at most +15.31 ms for independent
producers, +20.30 ms for bundle producers, +40.25 ms for readers and +25.73 ms
for complete bundles. The largest observed paired RSS ratio is 1.1451.
Results and qualifications are in `I5_VALIDATION.md` and `i5-survey.json`.
The I4 small-link memory exception remains specific to those reviewed costs;
new analysis modes retain the original memory gate.

## Independent review follow-up

B1/B2: diagnostic listing rows, instruction metadata, storage widths and recovered
syntax-error statements are preserved; Devil World's `render_sequences` reader
handles null offsets and compact fills, with good and corrupted setup controls.
S1/S3/S4: alias depth matches v1, substituted strings are not address references,
and unused bank expressions stay unknown without changing assembly acceptance.
S2: `.char` grouping matches v1; corrected numeric-char text and redefined-label
rows are listed in contract section 15. S5: fatal evaluator allocation failures
are checked after serialization, and file publication is staged/rolled back as
one invocation. S6: text-only listings no longer rebuild collector expressions.

Remaining optional items from review: rich xref still expands shared expression
graphs into trees (also expensive in v1); the bundle validates schema/hash/row
shape but does not compare every listing byte to the PRG; `zeropage_y` is still
missing from the pre-existing consumer index-mode set. The fixture migration
notes Zelda's four final-opcode `absolute` to `zeropage` corrections. Parser
allocation failures shared with v1 remain open. Independent review also found
inherited summary, text scope/source and xref data-provenance allocations that
silently lose metadata, plus index/consumer/coverage failures without explanatory
diagnostics. File staging does not fix ignored errors. The validation record
also records the syntax-error/NL diagnostic-listing restriction and I4's unused
label-warning difference through constant-alias chains.
