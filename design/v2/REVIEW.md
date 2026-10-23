# D1 local review record and reviewed scope

Status: **independent local review cleared; D1 approved by Kent Hansen on
2026-10-23.** The owner answered "yes" to approving D1 and publishing it as one
PR, merged after green CI. The approval record is in contract section 8.
The final reviewer verdict was: "no blockers. Both fixes are correct, and I have
nothing left that should hold up D1." It covers the semantic contract,
performance protocol, 15-ROM frozen gate, wire draft/example and D1 migration
ledger. Reviewer clearance and the subsequent owner approval are separate
events; implementation follows D0's staged gates after this revision is merged.

The three optional implementation-test cases (`.exitm`, a macro-produced label,
and redundant `.codeseg`) are now explicit obligations in contract §7. Their
semantics were already reviewed; no runtime source or captured evidence changed.

The historical review request and evidence scope follow for traceability.
The pending-approval wording below describes the request before approval.

Please review the local branch `design/v2-contracts` at
`/private/tmp/xorcyst-v2-contracts`, against master `162dacc`.
Nothing in this batch is pushed. This is a design/evidence review, not a request
to start implementing v2 or approve a release.

Read [XORCYST_V2_CONTRACTS.md](../../XORCYST_V2_CONTRACTS.md),
[PERFORMANCE.md](PERFORMANCE.md), and [BASELINES.md](BASELINES.md), with the
approved D0 document as the scope boundary. The scripts only capture/test v1
evidence. No assembler/linker/parser/scanner runtime sources have changed.

This is the focused scan-default and import-alias clarification. RAM ordering, allocation extents,
the 15 ROM gate, first-walk narrowing and earlier baseline fixes are unchanged.
Please check:

1. Semantic §4.1 now names the remaining scan boundaries: `.charmap`, data
   `.align`, `.union` and `.record`; `.ifndef`, `.else`, `.elif` and `.do`/`.until`
   contribute their selected/expanded statements. After the data/storage, label
   and internal-marker rules, any other statement still present ends the scan.
   Already consumed statements are absent; expansions are judged by what they
   leave. The prior include/macro distinction and RAM capture rules are unchanged.
2. The kind rule and second D1 ledger row explicitly cover local aliases ending
   in imports, whether the alias is defined before or after the table/sizeof.
   Both v1 controls link to `07 02`; both deliberately reject under the selected
   v2 policy. Local forward numbers/addresses still pass their unit-end kind
   checks. No scan retry, linker check or wire guard is added.

The previously reviewed string-definition-order correction and explicit-address
`.label` rejection are unchanged. Extent tag 7 remains invalid, and the unchanged
244-byte fixture is the only wire example. The owner choices do not approve D1
or authorize implementation.

The new evidence is `2026-10-23-sizeof-boundaries/observations.json` beside the older
capture, or `sizeof-capture-observations.json` in this worktree. It records 50
cases × 4 configurations, all assertion-checked on v1 with no syntax/harness
errors. The explicit-address-label scan is copied as `fixed-label-scan.json` there;
the previous reports remain unchanged in `2026-10-23-sizeof-capture/` and
`2026-10-23-sizeof-scan/`.
The v2 expected results are contract assertions; there is no v2 implementation.

The selected performance rule is unchanged: median <=1.000 and no independently
confirmed regression. Truly equal performance can still remain unresolved due
to noise; such a result returns for review, not automatic approval.

The local evidence root is
`/Users/kenthansen/dev/xorcyst-v2-baselines/2026-10-22-capture/`.
Use its `capture.json`, `manifests/`, `logs/`, `inputs/`, `analysis/`,
`consumer-evidence/` and `contract-cases/`. The repository contains hashes and
observations, not private project sources or ROMs. Old scratch captures are not
the baseline. Dpadhero's seven precompiled song objects remain excluded; do
not spend time recovering/converting them or supporting the old format.

The revised gate is beside that archive in `2026-10-22-frozen-assets-02/`;
review `gate.json`, frozen asset hashes, both environments and logs. Historical
capture paths are preserved as provenance, not runnable relocated bundle manifests.
Wishmaster converter reproducibility is explicitly not claimed. The revised
contract cases now live beside it in `2026-10-22-contracts/`. Check the listing-null migration
and declaration-time rejection (including unused types and fixed-PC aliases).

New evidence is beside the capture in `2026-10-23-wire-review/`; read
`observations.json` and `review-tools/stable-sort.diff`. The source-tree
`ram-span-observations.json` normalizes scratch paths for readability. Old attempts
are segregated under `superseded/`; the historical capture is no longer appended.
The release tarball and installed docs no longer contain broken baseline links;
they name the source-only evidence path as text.

The [separate wire draft](OBJECT_WIRE_DRAFT.md) follows the semantic corrections.
Check every logical entity's concrete representation, count/extent validation,
source locations, shared references, character maps and import ownership. The
hand-written `object-wire-example.hex` is independently inspected by
`python3 -B design/v2/check_wire_example.py`; it is not a production encoder or
loader. Its numerical tags remain proposed until the wire review is complete.
No implementation is authorized until both checks are complete and the owner
approves D1.

Report blockers separately from optional improvements. For any additional
feature, identify the current project dependency or reproduced failure it
addresses. Do not expand the approved scope merely to regularize v1 behavior.
