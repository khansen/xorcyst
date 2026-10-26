# 2.0.0 release coordination

Updated 2026-10-26. The owner approved I6 at `7105ea7` and has now authorized
creating the 2.0.0 release. Publish the prepared metadata through a PR, merge
on green CI, and build the release archive from that exact merged commit.

## Public producer and tooling: merged

- XORcyst [#102](https://github.com/khansen/xorcyst/pull/102) merged as
  `9a273fa081524e3d55d608b3c9587ad55261aca4` after Ubuntu, macOS and
  ASan/UBSan CI passed. Its post-merge CI also passed.
- Nesrev [#171](https://github.com/khansen/nesrev/pull/171) merged as
  `5b9bf953c0e62173173016b0102e28ec797b0c2e` after Ubuntu and macOS CI passed.
  Its CI pin names the actual XORcyst merge above, not the reviewed branch SHA.
- A fresh build of the merged XORcyst tools passed all 33 bundle tests with
  the paired public tooling. No private projects branch or ancestry was pushed.
- CI exposed a pre-existing watcher-test cleanup race. The two tests now keep
  their watchers as direct children and wait before removing their files.
  Both pass locally; the old launch fails the new lifetime assertion in both
  scratch controls. Production watcher code is unchanged.

## Private projects workflow: full rebase activated

The owner requested a full rebase onto public master, replacing the earlier
cherry-pick integration plan. All 1,189 private commits replayed without
conflicts onto Nesrev `5b9bf953c`. The reviewed private consumer changes were
then retained as a separate commit. The prepared result is
`local/2.0-projects-rebased` at `3b471603b5222fda5bce716471daecf506bb5c16`.
It is local-only and must never be pushed.

Before adding the consumer changes, every project file matched the original
`projects` tree. After adding them, every project file and the listing-consumer
test matched the reviewed integration at `1ec16789e`. The four consumer fixes
were developed on the public-master-based branch at `b240aa806`; this rebase
adds no new project tooling behavior.

The rebased workflow passes all eight real producer setups, with five exact
reference PRGs, including Devil World's renderer with a synthetic palette.
The two listing-consumer tests (16 subcases) and all 33 bundle tests pass.
No emulator was launched. Strict playbook checking still reports ten stale
AGENTS references in historical project review archives; those same ten
warnings are present in the original project files. This is not a strict-docs
pass and no archive history was rewritten to hide the warnings.

A complete Git bundle of the original projects history and earlier migration
branches is retained outside both repositories. The live `projects` checkout
now uses the verified full rebase at `3b471603b`; all five untracked files are
preserved byte-for-byte, and the branch has no upstream. Nothing private was
pushed. Both system-installed tools report 2.0.0, and their disassembled code
matches the reviewed release build. The eight producer setups were repeated
from the live checkout with those installed tools: all five reference PRGs
match, listing schema 2 is validated against their bytes, and Devil World's
renderer passes with a synthetic palette. No emulator was launched.
The live checkout also passes the two listing-consumer tests (16 subcases)
and all 33 analysis-bundle tests with the installed tools.

## Remaining release gates

1. Publish the owner-approved metadata PR and merge on green CI.
   Build the release archive from the exact merged tree, verify its contents
   and checksum, then tag and publish v2.0.0 as a normal release.
2. Remove completed integration branches/worktrees after their contents are
   preserved and the corresponding gates have passed. Keep unrelated unmerged
   feature work and uncommitted changes. Never push `projects` or any branch
   containing private project history.

## Local release preparation checks

The release metadata removes candidate wording from NEWS, README, the migration
guide and the manual. The completed migration records were removed from the
working tree; their history is preserved in Git. Maintained object-format and
performance documents now live in `doc/`, project checks and portable fixture
identities in `tests/projects/`, and the wire example in `tests/fixtures/`.
The root semantic/design contracts remain. No assembler or linker source changed.

At `05f863c`, Automake 1.19 regenerated the packaging rules and Texinfo 7.3
regenerated the manual. `make distcheck` passed 531 tests (19 regression suites
plus installed-tool checks), shell regressions, installation, uninstall and
distclean, with no skips.
The first sandboxed run could not bind the socket used by an output-safety test;
the successful run had the required permission. The install tests also verify
the two relocated public documents.

The moved project runner rebuilt all 15 required ROMs from 451 source-built
objects byte-for-byte. Its executable Python syntax is unchanged, as are the
other four project tools. The independent 244-byte wire fixture is byte-identical;
its checker and all five project-tool imports work from the source archive.
Local document links and frozen-input identities were checked.

Independent local review cleared that cleanup and the subsequent edits to the
migration guide, TODO and fixture provenance. The follow-up also clarifies
where validation evidence is retained. Only documentation and provenance text
changed; all 50 sizeof cases, runtime sources and build rules are unchanged.
The archive was regenerated with `make dist` and checked against the committed
source. The full suite and ROM gate were not repeated for these text-only edits;
the results above apply to the unchanged code and tests.

The subsequent TODO update tracks integration of the preserved reviewer tools.
The tracked, source-only `DEVELOPMENT_STRATEGY.md` defines that future work; no
harness or CI changes are implemented by this documentation update. `make dist`
was repeated for the shipped TODO change, and all archive files were compared
with the prepared source. Full tests and project builds were not repeated.

All 184 regular files in the archive match the prepared tracked source. No
historical `design/` directory, private evidence or owner-local filesystem paths
are included. The local archive, SHA256SUMS and validation record are outside the
worktree under `/private/tmp/xorcyst-2.0-release-preparation/`; earlier archives
are retained in `previous-2181add/`, `previous-05f863c/` and
`previous-c436394/`. None is committed or
published. The current archive SHA-256 is
`fc6019eff0cedca0af1c4ac0d7bf9c0925a5f4f8354a9ff77f32358af9dc745a`.

These checks validate the prepared archive, not a tagged release. Rebuild and
verify from the final merged release commit before publication.
