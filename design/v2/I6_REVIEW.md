# I6 local review: coordinated 2.0.0 cutover

Review this candidate before any I6 PR, push or release tag. I1–I5 approvals are
historical evidence, not approval of this cutover. The owner requested one
implementation, no legacy object reader or selectable old listing schema, and
normal `xasm`/`xlnk` commands. The paired Nesrev branch starts at public master;
never publish the projects branch or its history.

## Scope

Compare XORcyst against I5 merge `6095465`, and Nesrev against `30b8382e5`.
Read `I6_VALIDATION.md`, `I6_TEST_MIGRATION.md`, the migration guide and the
approved contracts first. The final commit IDs are in the handoff message.

- `ir/` replaces `v2/`. Private APIs are named `ir_*`. Normal build/install
  targets use the shared frontend, evaluator, layout, object codec and linker.
  `codegen`, `unit`, `objdef`, `hashtab`, old layout/emission passes, compile-time
  engine switches and obsolete storage masks/string-fold branches are removed.
- Structured listing schema 2 is the only writer; `--listing-version` is
  removed. Instruction records stay schema 3. Existing schema identifiers for
  other products are unchanged. Historical design/release documentation remains.
- The parser, expansion rules, fast integer folding and shared analysis
  renderers remain; this is not a replacement of working source semantics.
- Existing expression controls exposed unresolved aliases bypassing the AST's
  undeclared-name walk. The unit-end capture check now rejects missing `@@`
  locals and warns for implicit external aliases. Audit source location,
  scope isolation and warning duplication; plain and object paths are covered.
- `xlnk --verbose` now reports linked/copied/published file paths. The prototype
  had accepted it without useful output. The old RAM symbol-map dump is not
  carried forward; its allocation test now asserts the actual operand bytes.
- Normal source/archive/install tests replace representation-specific bytecode
  assumptions. The migration map names the replacement coverage. Check for
  accidentally weakened assertions, especially old error-stage assumptions.
- Nesrev removes the listing selector from five shared producers and updates
  its producer pin/docs. No project files are in the public tooling branch.

## Review priorities

1. Confirm there is one active engine and no format-1 writer/reader or hidden
   selector. Check API renames, conditional removal and build/archive coverage.
   `astproc.c` is included by `ir/frontend.c` and explicitly distributed.
2. Check output and diagnostics against approved I5 where behavior should be
   unchanged. Examine the two captured-alias diagnostics and verbose CLI change.
3. Check default listing rows/joins, analysis schemas and diagnostic listings;
   preserve null non-emitting offsets, fills and accumulator addressing.
4. Check all required ROMs, object identity and the paired Nesrev producer/
   reader path. The dpadhero2 gate cannot be waived. The old dpadhero song
   objects remain excluded; do not recover them or add an old-format reader.
5. Check package/install/uninstall behavior from the archive. No prototype
   executables are installed. The `ir/` name avoids Autoconf's reserved crash
   dump filename `core`. Inspect the release and migration documentation.
6. Review the bounded resource survey and retained limitations. Investigate
   material regressions or simple useful savings; do not spend unbounded effort
   on accepted millisecond costs or add machinery solely to save a few KiB.

Use scratch builds and copies of frozen fixtures. Do not edit source projects,
reference ROMs or the frozen archive. Source/runtime test changes require the
appropriate rerun; docs-only evidence need not trigger a full unrelated audit.

The validation record separates final-head checks from earlier exploratory
failures. No PR CI, 32-bit or big-endian host result is claimed. The Linux codec
suite does exercise its word-conversion helper. Existing allocation/collector,
RAM-PC and verbose-output limitations are described rather than concealed.

Report blockers separately from optional improvements. A review is not owner
approval. After approval: publish XORcyst first, run CI and merge, then re-pin
the master-based Nesrev tooling branch to the actual merged producer commit
before publishing it. Verify both merged heads and complete the private
projects-workflow gate in `I6_RELEASE_CHECKLIST.md` before release or system
installation. No tag or system installation is authorized by the local-review
handoff itself.
