# I6 review follow-up

2026-10-26. Independent local review cleared `8e35489` and paired Nesrev
`c6879021e` with no blockers. Owner approval of I6 and publication is still
pending. The owner separately approved preparing and verifying the private
projects migration locally before 2.0. Nothing was pushed, tagged or installed
system-wide.

## Cutover cleanups

- Restore the supported option list in `xlnk --help`/`--usage` and regenerate
  `man1/xlnk.1`. This help string is the only executable-source change.
- Document the shared linker's less-specific diagnostics, its distinct-output
  requirement, and refusal to copy an output of the same invocation. These are
  existing I4 behaviors, not new linker policies in this follow-up.
- Correct the captured-alias diagnostic description to say **alias definition**,
  rather than use. Pin the location for missing scoped locals and for an
  implicit external alias, including no duplicate warning. Diagnostic order is
  unchanged and its reverse-order possibility is documented.
- Remove the dead `if False` test branch, retaining the active failure and
  output-preservation assertions.
- Replace personal checkout paths in the shipped design document with project
  names and relative entry points; fixture identities and requirements stay
  unchanged. The archive no longer contains those personal design paths.
- Add `I6_RELEASE_CHECKLIST.md`: Nesrev must be pinned to the **actual merged**
  XORcyst commit before its PR is published, regardless of merge strategy.
  The current local pin is review-only. Record private workflow activation as
  a separate prerequisite for installation/release.

## Private tooling and integration

New tooling work was done on `local/2.0-project-tooling` (`b240aa806`), based on
the public-master-derived `c6879021e`. It holds four project consumers copied
from the reviewed I5 port plus its listing tests, with no listing selector.
The commit contains project files, so it is **local-only** and must not be
published even though it has no private project ancestry.

The scratch integration branch `local/2.0-projects-integration` (`1ec16789e`)
starts from the live projects commit `abc8ae3e1`. It integrates public #170
(`7b3659853`) and the I6 shared tooling (`35cf95cd5`), then the four consumer
ports and their tests. Relative to the prior private project tools, the actual
consumer change is 17 added / 4 removed lines across four files. It handles
non-emitting and compact-fill rows while preserving ROM-byte verification.
The other five of the eight trace producers already omit the removed selector
and need no edit. No assembly, project semantics or reference ROM is changed.

The live projects checkout, its existing untracked files and its branch tip
remain untouched. Activation is coordinated with the future tool switch;
advancing it now would pair schema-2 readers with the installed 1.9.12 default.
The release checklist records the final integration and path/version checks.

## Focused checks

Evidence root: `/private/tmp/xorcyst-i6-followup/`.

| Check | Result | Log |
| --- | --- | --- |
| Build | Clean normal rebuild; only linker help changed | `build.log` |
| Expression source suite | 35 tests pass, including new location assertions | `expressions.log` |
| Output failure suite | 23 tests pass unrestricted; sandbox run could not bind its Unix-socket fixture | `output-failures-unrestricted.log` |
| Linker CLI/output suite | 7 tests pass | `linker-outputs.log` |
| Help/man page | Every supported option is described; man page generated with help2man | `linker-help.txt` |
| Package/scratch install | Archive built, five installation tests pass, uninstall leaves no files; archived design has no personal paths | `dist.log`, `installcheck.log`, `uninstall.log` |
| Four project consumers | Two methods / 16 subcases pass: null offsets, fills, corrupt bytes and stale rows | `project-listing-tests.log` |
| Original private consumers | Same controls fail with the original null-offset defects (2 failures, 14 errors) | `project-parent-proof.log` |
| Shared tooling in private integration | Bundle harness and consumer-audit (15), embedded-pointer (9), pointer-table (23), listing-consumer (2) suites pass | `project-shared-tests.log` |
| Real project setup | All eight producers prepare successfully using 2.0; exact PRGs for Clu Clu Land, Devil World, Popeye, F1 Race and Hogan's Alley | `project-setups.log`, `project-setups/report.json` |

Devil World's capture runner is stopped at the emulator-launch boundary after
its real assembly and semantic setup checks. Its renderer consumes that real
setup with a synthetic palette record. No emulator execution or gameplay
evidence is claimed. The remaining scripts use their prepare-only paths or
their assembly helper. All outputs and copied reference ROMs are in scratch.

`i6-followup-validation.json` records current source/tool hashes and this
follow-up's evidence. The earlier `i6-validation.json` and resource/artifact
record remain historical evidence for `8e35489`; their old hashes are not
claimed to describe changed help/docs/tests. The assembler binary is unchanged.
No repeat of full distcheck, Linux/sanitizers, the 15-ROM gate or timing was
needed for this bounded follow-up. Their reviewed I6 results still apply to
the unchanged assembly/link engines; new behavior is limited to help output.
