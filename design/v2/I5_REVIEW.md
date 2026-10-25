# I5 local reviewer prompt

Review status: independent local review cleared `63e4ea5` / Nesrev `4ca95e06d`
with no blockers. The prompt below is retained as the review scope. Subsequent
documentation records the optional observations and inherited collector failures;
it changes no runtime or tests. Owner approval is still required before a PR.

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

## Focused re-review

The first review of `21fae67` / `ab50ea8f7` found B1/B2 and S1–S6.
Please verify their closure, especially diagnostic listings with no other
analysis product, first-pass and syntax errors, 32/64/65-level aliases, string
xref substitution, unused bank constants, and text-only shared-alias memory.
Exercise `actor_render.render_sequences` as well as the three trace runners.

Challenge invocation-wide publication: fail generation after binary staging,
fail late value evaluation and source capture, fail the second CSV/NL rename,
and check both existing and absent destinations. Old outputs and symlinks must
survive; temporary files must disappear. Rollback links are not a crash-atomic
commit, stdout is not reversible, and a rollback I/O failure must be reported
with its backup retained. Comparison/audit findings keep their existing valid
artifacts and exit statuses. Contract section 15 records metadata corrections.

Text listings should now pay only for source rows and symbol values. Rich xref
still has the pre-existing shared-graph-to-tree expansion cost; no fix for that
optional item is claimed. Likewise, no fix is claimed for shared parser or
collector allocation failures (summary, text scope/source, xref provenance and
missing index/consumer/coverage diagnostics), consumer `zeropage_y` indexing, or full listing/binary
byte comparison in the bundle reader. Note the four Zelda final-opcode mode
corrections in the fixture migration. The validation record separates this
follow-up's checks from earlier-head results.

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
