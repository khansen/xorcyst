# I6 coordination before release and installation

Updated 2026-10-26. The owner approved I6 at `7105ea7`. The final release
metadata is being prepared on a local branch; the owner has explicitly asked
not to open the 2.0.0 release PR yet. No tag or release is published.

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

## Private projects workflow: full rebase prepared

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
branches is retained outside both repositories. The live checkout remains at
`abc8ae3e1`; its five untracked files are preserved. System-installed tools
still report 1.9.12. The root-owned `/usr/local/bin` installation requires
administrator authorization.

## Remaining release gates

1. Complete local review of the release metadata. Do not open the release PR,
   push the release branch, tag, or publish until the owner resumes that work.
2. Recheck the live projects head and local changes. Activate the verified full
   rebase without overwriting untracked files, and switch the installed tools
   together. Preserve the old tool installation for recovery. Never push
   `projects` or any branch containing private project history.
3. Verify the actual default `xasm`/`xlnk` paths and versions, the listing
   consumers, and the exact project PRGs after activation. Preparation alone
   is not activation, and a PATH assumption is not evidence of installation.
4. After release approval, publish the metadata PR and merge on green CI.
   Build the release archive from the exact merged tree, verify its contents
   and checksum, then tag and publish v2.0.0 as a normal release.
5. Remove completed integration branches/worktrees after their contents are
   preserved and the corresponding gates have passed. Keep unrelated unmerged
   feature work and uncommitted changes.

## Local release preparation checks

The release metadata removes candidate wording from NEWS, README, the migration
guide and the manual. Texinfo 7.3 regenerated the affected HTML. No runtime,
tests or build rules changed from the approved cutover.

`make distcheck` passed 531 tests (19 regression suites plus installed-tool
checks), shell regressions, installation, uninstall and distclean. All 176
regular files in the resulting archive match the prepared tracked source.
The local archive and SHA256SUMS are outside the worktree under
`/private/tmp/xorcyst-2.0-release-preparation/`; they are not committed or
published. The archive SHA-256 is
`090f6c5bb2434a3e8ced0ee803389fa57b98d84dfdff98ddb80b7ea3be98bdef`.

These checks validate the prepared archive, not a tagged release. Rebuild and
verify from the final merged release commit before publication.
