# I5 local reviewer prompt

Review the complete local producer/consumer migration, not just its earlier
source-capture and listing checkpoints. This is a request for independent
review, not an approval or permission to publish. No I5 PR has been opened.

Producer: `/private/tmp/xorcyst-v2-analysis`, branch `feat/v2-analysis`, compared
with I4 merge `63747edf648aed016928c4e2671c7f4ad4236c37`.
Consumer: `/private/tmp/nesrev-xorcyst-v2-analysis`, branch
`feat/xorcyst-v2-analysis`, compared with `abc8ae3e1`.
The validation record and `i5-current.json` identify the tested files, tools
and evidence. Use scratch copies for probes and builds. Do not modify the
original project trees, frozen inputs or baseline archives, and do not commit,
push, open a PR or make other GitHub writes.

Read `XORCYST_V2_DESIGN.md`, `XORCYST_V2_CONTRACTS.md`,
`design/v2/PERFORMANCE.md`, `v2/README.md`, `I5_WORK.md` and
`I5_VALIDATION.md`. In Nesrev, read `ANALYSIS_BUNDLE_SPEC.md`, the migration
plan and the applicable tooling/quality playbooks.

## Scope and ownership

All existing pure-binary products are connected: listing, xref, instruction
records, summary, index patterns, data consumers, coverage, comparison,
raw-address audit, dependency manifests and FCEUX exports. Structured listing
uses version 2, xref version 2 and instruction records version 3. Object-mode
structured analysis remains unsupported, as approved. Installed tools remain
v1; I6 removes the old engine, prototype names and selectable legacy schemas.

The implementation reuses the collectors through an owned metadata view.
Native statements transfer out of the parser tree after lowering; marker and
symbol metadata have independent owners. Source paths are interned in the value
unit. The original namespace and remaining parser tree are destroyed before
layout. Final rows take their positions, lengths, opcodes and bytes from layout
facts, and expression/term evaluation uses the shared value graph.

Please challenge that boundary: look for borrowed symbol entries or paths,
misidentified definitions after reassignment or `.undef`, accidental dependence
on the old evaluator, and unnecessary retained copies. Cover macros, local and
anonymous labels, procedures, flattened user types, data grouping and incbin.
Source provenance and frontend graph-cache IDs must stay independent. Rich
source spans are allocated only for products needing them.

Check that facts describe the bytes for deferred strings and captured maps,
assignment generations, `$`, branches, narrowing cascades, RAM/non-emitting
rows, compact fills and origins. Compare standalone with embedded records,
and data/reference/owner/term identities across outputs. Check plain assembly
and object/link behavior as well: analysis must not alter emission or binding.

## Consumers and publication

Nesrev now requests listing 2. Non-emitting offsets are null; emitted end
offsets remain inclusive. Label positions come from xref definitions joined
to source identity. Byte validators expand compact fills. The physical 16 KiB
PRG page is distinct from CPU address and linker bank identity.

Bare and explicit accumulator shifts/rotates report `accumulator`. Some 6502
references group that mode under implied addressing; the schema uses the
distinct value, and register-use analysis must model its effect on A.
Audit remaining direct JSON readers, standalone invocations and bundle paths.
Historical committed reports are retained as historical evidence; current
bundles must be regenerated, not silently reinterpreted.

Publication must fail closed on incomplete facts, malformed input, allocation
or I/O errors. Exercise prior-output preservation and output/input aliases,
including auxiliary NL/CSV/manifest destinations. Diagnostic listings may be
published only through the existing safe exception, marked unsuccessful with
no emitted bytes or offsets. They cannot certify a bundle. The bounded legacy
expression view now rejects an unrepresentable expression instead of silently
dropping references; this must not restrict plain assembly.

## Cost and validation

Assess the resource report honestly. A bounded survey is not a formal
no-slowdown pass, and an improvement in a consumer does not automatically waive
a producer regression. On 2026-10-25 the owner explicitly chose to document
the measured small I5 costs (producer overhead up to about 15 ms in the initial
survey) for local review and focus further work on material regressions. See
contract section 14; this is not I5 approval or a universal timing allowance.
The original per-configuration memory gate still
applies to analysis; I4's small-link exception is specific to I4. The owner
asked for absolute milliseconds/bytes and bounded optimization effort, not
elaborate machinery to shave tiny costs. Look for simple measured improvements,
unexplained growth and pathological inputs. Do not infer a new tolerance.

Check the 52 producer configurations, all eight bundle profiles on frozen
Metroid/Zelda, the 15-ROM gate, sanitizer runs, allocation/I/O failures and
Nesrev negative controls. Reproduce representative results independently;
author and mutant logs are evidence, not an oracle. Inspect the tests for
unasserted or unexercised paths. Do not claim native big-endian/32-bit, emulator
or PR-CI coverage: none is supplied by this local candidate.

Report concrete source/command repros, expected and actual behavior, origin
(introduced/pre-existing/approved), consumer impact and test coverage. Distinguish
blockers from optional improvements and retained I4 limitations. I5 approval
and subsequent publication remain the owner's decision.
