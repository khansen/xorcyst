# D1 frozen baseline record

Captured on 2026-10-22 with a fresh archive build of v1.9.12,
commit `86c1a43d1dfa237facf40d75085e9112e8617a13`. This is a **v1 baseline**,
not v2 implementation, compatibility, or performance evidence.

The owner-local evidence is retained outside temporary review directories at:

`/Users/kenthansen/dev/xorcyst-v2-baselines/2026-10-22-capture/`

The archive and earlier capture attempts have been moved outside the
repository, so `git clean -fdx` cannot remove them. Historical `capture.json`,
commands and bundle manifests retain their original paths and bytes as provenance;
those old absolute paths are not runnable commands after relocation. Private
project sources and ROMs remain outside the XORcyst repository.

The authoritative **frozen-asset gate inputs** and revised verification are in:

`/Users/kenthansen/dev/xorcyst-v2-baselines/2026-10-22-frozen-assets-02/`

Its `gate.json`, per-file manifests, logs and two environment records supplement
the historical capture. Earlier incomplete gate attempts are retained under `superseded/`, not treated
as baselines. The later 25-case semantic probe run is now beside the capture in
`2026-10-22-contracts/`; no future evidence should be appended to a completed
capture. Historical recorded command paths remain unchanged.

[baseline-index.json](baseline-index.json) contains full SHA-256 hashes,
Makefile targets, actual product paths, source locations, compiler information
and tool hashes. [manifests/](manifests/) records every captured input file,
including local modifications and shared-source contents. The capture report
itself is hashed by the index. Keep the local archive and these manifests
together; a hash without the frozen input is not a reproducible fixture.

## Source-built ROMs

Each of these 15 required builds succeeds from the same frozen source and
generated assets in two independent copies, with different HOME/TMPDIR/tool paths
and explicit minimal environments. Both complete ROMs match the original capture.
No `.o` or `.nes` is an input. `make -B -j1` rebuilds assembler/linker products;
`make -o` disables every converter target, and failing converter stubs catch
accidental regeneration. Input hashes are checked before and after the builds.
MD5 below is for comparison; SHA-256 in the index is authoritative.

The original two capture runs shared an environment and **did not prove converter
reproducibility**. In particular Wishmaster's `song2.inc` changes across environments
with the same xm2nes binary and input. Its captured `song.inc`/`song2.inc` are now
fixture inputs, verified by the complete ROM below. This gate does not claim that
regenerating them yields a unique or musically correct result. It compares v1 and
v2 assembling the same bytes. All other converter products are frozen too.

| Fixture | Bytes | MD5 |
| --- | ---: | --- |
| bombsweeper | 24,592 | `b33782b3b961b6fbb8b4aad3a37e3b7e` |
| dpadhero2 | 262,160 | `841bfe4ef75d0e274ece167cfbd4b8ee` |
| ntxdev | 262,160 | `0ac03c2493892df178053ae553d8a6db` |
| bob-omb-battlefield | 24,592 | `a6bb993d911f141e0ae931304d65d470` |
| dire-dire-docks | 24,592 | `55f8869b476983b05430bbecf0d1098c` |
| double-dragon-ii-vs-f1 | 24,592 | `66639e866c14568d5c05e664bde100d4` |
| dpadhero3proto | 24,592 | `ccac5baadab91f3f7a3f26989666aaa2` |
| is-it-scary | 24,592 | `90638d8b56ea946a9fc6bea0afd91497` |
| live-and-learn | 24,592 | `f4acd9221c4b3bc4ec68efbbf1f57707` |
| mega-jean-trooper | 24,592 | `f5cfea9940166e36b204732c28cd63a9` |
| merry-xmas-everybody | 24,592 | `2164cb93700ac77b7e8fb8ef8b25c383` |
| somewhere-only-we-know | 24,592 | `5bd020b544155d04ea6ac543a55d81bd` |
| there-must-be-an-angel | 24,592 | `70e32a281da506eb93e79c0f3670b0ad` |
| welcome-to-paradise | 24,592 | `b7bec420a8ccb8c8820755aa051d727c` |
| wishmaster | 40,976 | `689e80235343bc0a34e7ae704a75e3b7` |

dpadhero2 includes the owner's `src/game.asm` edit; ntxdev includes `warp2d.h`.
The music snapshots include working-tree/untracked projects and source assets;
the verified generated targets are frozen as an explicit overlay. These inputs must not be replaced with Git HEAD
merely to obtain a clean checkout. The capture rehashed included original files
after building and found them unchanged. Builds only ran in copied trees.

Two naming details matter:

- dpadhero3proto's target/product is `DPadHero3Proto.nes`; its dated leftover ROM
  is excluded.
- there-must-be-an-angel's Makefile target is `WelcomeToParadise.nes`, while
  `link.s` writes **`ThereMustBeAnAngel.nes`**. Record and verify that actual
  product after a forced rebuild; a successful Make invocation alone is not
  enough. This capture does not edit the project's old Makefile.

The dpadhero2 log retains implicit-external warnings at `common/timer.asm:84`
and `game.asm:1018`, both for `reset`. They are part of the baseline warning
policy. Dropbox dpadhero and its seven precompiled song objects were not read,
recovered, converted or built for this capture; its full ROM remains non-blocking.

## Toolchain and reproducibility

The baseline was configured with `--disable-maintainer-mode`, `YACC=:`, `LEX=:`,
`HELP2MAN=:`, `CC=cc`, `CFLAGS=-O2 -g -Werror`, and built with `make -j2` on
macOS arm64 using Apple clang 21.0.0 (`clang-2100.3.34.2`). The index records
the OS and compiler output, assembler/linker hashes and dynamic-library names.

Frozen converter executables: installed `xm2nes`, `xm2btn`, `packchr`, `packnam`,
and dpadhero2's separately selected `tools/xm2btn/xm2btn`. The two xm2btn hashes
differ, so they are not interchangeable in the fixture. Converters and generated
asset commands are retained, not folded into assembler performance claims.
The old dpadhero2 build deleted 23 intermediate `.inc` files. Two isolated converter
runs with `.SECONDARY` retained those files, produced identical asset hashes and
reproduced the original dpadhero2 ROM; the verified files then became gate inputs.
Wishmaster assets were copied from the original builds, never regenerated.
Future benchmarks rebuild both assembler versions with a matched current
toolchain; these Mach-O hashes identify this capture, not a portable compiler.

Reproduce the frozen-asset verification into a **new** directory:

```sh
python3 -B design/v2/freeze_gate_assets.py \
  /Users/kenthansen/dev/xorcyst-v2-baselines/2026-10-22-capture \
  /absolute/new/gate-directory
```

The command reads only frozen inputs and historical converter commands. It
rejects a reused destination, mismatching assets/ROMs or any converter invocation
in a gate build. Its child environment is a whitelist; no shell `XASM_TEST_*`,
MAKEFLAGS or dynamic-loader variables are inherited. Both environments and full
commands are recorded. Future v2 comparisons consume the already frozen gate
inputs, not live project trees or newly converted songs.

`freeze_baselines.py` remains a source/converter-provenance capture tool with a
minimal child environment, not a reproducible gate by itself. A new capture must
freeze/verify its assets separately. `index_baselines.py` records both reports;
`scan_inputs.py` scans the frozen source; `probe_contracts.py` captures v1 behavior.
These are local evidence tools, not a v2 backend or timing harness. Evidence,
private-path manifests and the reviewer prompt are intentionally absent from
installed documentation and release archives.

## Nesrev baselines and consumers

Nesrev revision: `260a50bd79a6e4a8a3f4104a3b928aee40d02154`. Scripts, contracts,
project configuration and the actual Metroid/Zelda sources are frozen. Each
source emits 131,072 PRG bytes, matching its corresponding reference-ROM PRG
after removing the 16-byte iNES header.

| Source | Frozen source SHA-256 | PRG SHA-256 |
| --- | --- | --- |
| Metroid | `9bb872dd7b0d619286230b8aade7828a3ac5ec67d3cb843f447bfd1edde873fb` | `d2cdc3e8432f104709c17c88b20ceae87780e97cf6a2e3372fd54d0a67972062` |
| Zelda | `d7ccb14787de91d92001bf0bacfdd3e4fa1a952ea950a9c758a8ca961ed69def` | `085e5397a3487357c263dfa159fb0fe20a5f3ea8ef82d7af6a7e848d3b9364e8` |

Both pass all eight frozen bundle profiles, including strict ci-data-v1 and
ci-instructions-v1. The actual bundle producer validated schema, dependencies,
instruction bytes and completeness. Each also succeeds in 15 independent modes:
plain; text/JSON-v2/NDJSON-v2 listing; xref; xref-data+owner; embedded records;
standalone records+manifest; summary; index patterns; data consumers; coverage;
compare; raw-address audit; and FCEUX. Every binary matches the reference PRG.
This is 46 mode captures, not 46 performance samples or a complete run of every
Nesrev consumer command.

[consumer-inventory.json](consumer-inventory.json) inventories the frozen
producer flag sites and 20 direct-reader candidates. It is a static inventory,
not proof that all consumer branches have been exercised. In addition,
`consumer-evidence/` locally freezes 394 tracked fixture/contract files, including
dated generated analysis fixtures and `pair_redefined_label`. The corresponding
manifest is committed; historical bundle paths are evidence, not reusable fresh
bundles. Keep the D0 named-reader list in addition to this syntactic search.

| Output / current consumers | D1 migration contract and check |
| --- | --- |
| Structured listing — analysis_bundle, data_directive_xref, consumer_audit, orphan_opcode_scan, static_analysis, embedded_pointer_audit, pointer_table_body_check.py | Move bundle acceptance from listing 1 to 2; check corrected fill/non-emitting rows and inclusive ranges, emitted bytes and address/offset joins. JSON/NDJSON remain. |
| Xref — analysis_bundle, used_by_xref_check, branch_literals, split_pointer_targets, ram_accesses, pointer_targets | Keep version 2 facts/options, member exclusions and deterministic order. Check symbols, definitions, references, data edges and scope/filter behavior. Text/CSV remain. |
| Instruction records — instruction_records and its callers | Keep version 3 file table, byte validation, operand values, terms, trees, bindings and access classifications. Standalone and embedded representations agree. |
| Summary — pass-prep and ranking readers | Keep filters, rankings, referrers and nearby-window meanings in existing text/JSON/NDJSON forms. |
| Index patterns — embedded/pointer/table readers, project wrappers | Keep displacement/bounds/split-pair facts and their source and symbol joins. |
| Data consumers / coverage — consumer_audit, extent/blob-disposition readers, pass-prep | Keep span/access aggregates and uncertainty; distinguish inclusive listing ends from existing exclusive coverage ends. |
| Compare / raw-address audit — verify, project_compare, project_audit, project_pass_prep | Keep slices, source-mapped mismatches, ranges, levels and output/exit policy. |
| Dependency manifest — analysis_bundle and cache users | Keep input/negative-lookup identity, completeness, producer hash and manifest-last publication; changed producer invalidates caches. |
| FCEUX — current export consumers | Keep RAM/ROM NL format, output-offset PRG pages, mirroring and collision protection. |
| Object structured listing/xref | Explicit unsupported-mode error at cutover; no current game/Nesrev consumer. No new linker analysis product. |

The frozen Metroid v2 listing has null offsets in 6,951 of 50,834 rows.
`analysis_bundle.py` currently requires integers there and must migrate; skipping
those rows blindly would lose source/extent information. Add mixed emitting,
fill and non-emitting row fixtures, preserving nulls without inventing offsets,
and test joins in `pointer_table_body_check.py` (offsets, addresses and bytes).
`orphan_opcode_scan.py` already tolerates nulls; retain that positive control.

All D0 products have a disposition above; no existing flag is dropped merely
because it did not appear in one bundle profile. CLI filters, file names and
warning policy still need their existing tests in I5. The 15 mode capture does
not replace that coverage or the performance matrix's alternate formats.

## Semantic probes and remaining gates

[contract-observations.json](contract-observations.json) records 25 small v1
cases, their exact source, statuses, diagnostics and emitted/preserved bytes,
alongside proposed v2 results. It reproduces the relaxation controls, both
retained mode differences, type capture, forward strings/maps/exports, zero
field remainder and implicit externals. Exported-forward aliases fail during
assembly on v1, so there is no invented v1 link result. v2 expectations are
design assertions, not executed tests.

The focused wire follow-up is in `2026-10-23-wire-review/`, with
[ram-span-observations.json](ram-span-observations.json) as the source-tree summary.
`probe_ram_spans.py` checks nine contiguous label-data spans in both modes,
RAM unlabeled/zero-size extents and the exact permutation in one-unit, split-unit
and reversed-unit cases. It also relinks all 15 frozen object sets: v1 matches
every ROM; the reviewer's stable-sort linker changes exactly the 14 reported ROMs
(dpadhero2 1,031 bytes; ntxdev 10,735). The archived mutant binary is hashed and
its source-only diff retained. This is v1 compatibility evidence, not v2 runtime
implementation or a v2 performance result.

Reproduce in a new scratch directory with the frozen tools and optional stable
mutant, for example:

```sh
python3 -B design/v2/probe_ram_spans.py \
  /Users/kenthansen/dev/xorcyst-v2-baselines/2026-10-22-capture/tools \
  /absolute/new/wire-probes \
  --gate /Users/kenthansen/dev/xorcyst-v2-baselines/2026-10-22-frozen-assets-02 \
  --stable-linker /Users/kenthansen/dev/xorcyst-v2-baselines/2026-10-23-wire-review/review-tools/stable-xlnk
```

The current follow-up for assembler-time label sizes and scan boundaries is in
`2026-10-23-sizeof-boundaries/`, summarized by
[sizeof-capture-observations.json](sizeof-capture-observations.json).
`probe_sizeof_capture.py` pins 50 v1 cases in binary/object modes, debug off/on
(200 runs): both RAM sizes remain 2, forward label/count uses reject during
assembly, and known strings/counts retain their captured sizes. Consumed
declarations and expanded conditionals/loops/macros keep the scan going, while
public/type/label declarations and includes stop it. Data-only includes differ
from macro calls. The additional controls cover `.charmap`, data `.align`,
`.union`, `.record`, `.ifndef`/`.else`/`.elif` and `.do`/`.until`.
Rejected runs preserve their seeded output.

Forward/imported string cases record v1's wrong linked values separately from
the selected v2 size-unknown policy. A string defined between the data and sizeof
has the same wrong v1 bytes, but the recorded v2 correction is the known length
6. Local numeric/forward-pointer controls keep v1's bytes under the selected
unit-end kind check; explicit and implicit imported pointers deliberately become
size-unknown errors. Local aliases of imports link to `07 02` in v1 in both
definition orders and are rejected by the same selected v2 policy. These v2
outcomes are design assertions, not executable v2 tests. The previous reports
remain unchanged in `2026-10-23-sizeof-capture/` (15 cases/60 runs) and
`2026-10-23-sizeof-scan/` (40 cases/160 runs) as historical review evidence.

The old 376-byte span fixture is withdrawn: `sizeof(label)` must already be an
integer or assembly error, never a link-time span query. The original 244-byte
wire fixture still decodes unchanged. Historical RAM/span observations remain
valid evidence of v1, but do not authorize a link-time sizeof implementation.
A separate syntactic scan found no `.label name = expr` in 866 frozen assembly,
include and header files; this supports the selected exclusion, not a claim
about every possible macro expansion.

[pc-scan.json](pc-scan.json) found no direct `$` in type bodies or expansion
directives across the frozen assembly/include/header files. Planted positive
and hex/string/comment controls pass. This scan is syntactic; transitive
macro/alias dependencies still need the semantic tests in the contract.

Independent review and owner D1 approval are complete; see section 8 of
`XORCYST_V2_CONTRACTS.md`. These baselines are included in that approval.
No v2 implementation, candidate
ROM, timing pass, downstream migration, Linux/32-bit/cross-endian result, or
new release is claimed. The full v1 suite was not rerun just for these documents;
the fresh strict build and the real-project/bundle captures above are the local
checks. CI on the merged D0 documentation passed separately.
