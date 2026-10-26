# I6 coordination before release and installation

The independent review cleared XORcyst `8e35489` and paired public-master
Nesrev `c6879021e` with no blockers. This is not owner approval. The small
follow-up changes and their checks are recorded in `I6_REVIEW_FOLLOWUP.md`.

## Public producer and tooling

1. Obtain owner approval of the reviewed cutover before publishing its PR.
2. Merge XORcyst only after CI is green. Read the actual merge commit from the
   PR and verify that it is on `origin/master`. Do not assume the reviewed
   branch SHA survived a squash or rebase merge.
3. Before publishing Nesrev's tooling PR, set `XORCYST_REVISION` to that actual
   merged XORcyst commit. Verify the pin is an ancestor of XORcyst
   `origin/master`, and repeat the paired producer/consumer smoke checks with
   that checkout. The current local pin is for review only.
4. Publish only the Nesrev branch based on public master. Merge after green
   CI and verify the two merged heads. Never publish the private projects
   branch, its commits, or a branch containing its ancestry.

## Private projects workflow: prepared, activation pending

The owner's live Nesrev projects branch is still at `abc8ae3e1`. It has local
untracked files and has not received the shared listing-2 migration from #170
or the private project-tool changes reviewed at `4ca95e06d`. Installing 2.0
as the default assembler now would break some of its consumers.

On 2026-10-26 the owner chose to migrate locally before 2.0. The prepared
integration is `local/2.0-projects-integration` at `1ec16789e`, in
`/private/tmp/nesrev-2.0-projects-integration`. It contains the two reviewed
public tooling changes and the project consumer port developed separately on
the public-master-based `local/2.0-project-tooling` branch at `b240aa806`.
Both branches are local-only; neither may be pushed. The public tooling PR
branch contains no project files or private ancestry.

All eight producer setups pass with the explicit 2.0 tool path, all five
affected project PRGs are exact, and Devil World's renderer accepts the real
setup. Null/fill/corrupt/stale row controls pass. No emulator was launched.
See `I6_REVIEW_FOLLOWUP.md` for the evidence and scope.

Before installation/release, recheck the live projects branch for intervening
user commits or edits, integrate the prepared changes without overwriting them,
and switch the assembler and projects tooling together. The live checkout was
intentionally not advanced while its default assembler is still 1.9.12. This
activation step remains a release gate; preparation alone is not activation.

For a local migration:

- Develop any tooling edits on branches from public master. Do not develop
  them on the projects branch or merge its history into a public branch.
- Bring the approved shared tooling into a scratch copy of the projects
  workflow first. Preserve the owner's untracked files and project edits.
- Port the I5 listing-row fixes for Clu Clu Land, Devil World and Popeye,
  including Devil World's `actor_render.render_sequences`. Skip non-emitting
  rows, handle fill rows, and retain ROM-byte checks. These are four consumer
  files; no assembly or semantic project changes are needed.
- Do not blindly replay I5's eight trace-script command edits: they added
  `--listing-version=2`, which 2.0 rejects. Check all eight producers and ensure
  none passes the selector. The original projects versions already omit it.
- Run the listing-consumer controls, including null/fill rows and corrupt or
  stale setups, and exercise actual 2.0 assembly for the affected projects.
  Verify exact PRGs and rebuild disposable setup/bundle files. No emulator
  capture is needed merely to validate the listing migration.
- Only after those checks, integrate locally as authorized. Record resulting
  commit IDs and verification; never push the projects branch.

At activation, verify the resolved `xasm` path/version as well as scripts that
invoke it by name, re-pin to the actual public merge as above, and rerun the
bounded setup checks. A PATH assumption is not sufficient evidence of the switch.
