# Project compatibility and performance checks

These tools build only in new scratch copies. They require explicit assembler
and linker paths and private frozen inputs, which are not part of this repository
or release archive. They are optional maintainer checks, outside `make check`.

## Frozen inputs

[baselines.json](baselines.json) identifies the v1.9.12 source revision, capture
and gate manifests, source manifests, targets and exact ROM/PRG hashes. Keep the
complete frozen archive outside the repository so `git clean -fdx` cannot delete
it. The archive root is supplied explicitly; these scripts do not assume a
particular owner's filesystem path.

The archive contains `2026-10-22-capture/` (sources, tools, commands and Nesrev
inputs) and `2026-10-22-frozen-assets-02/` (the authoritative `gate.json`, inputs,
per-file manifests and verified generated assets). Historical commands retain
their original paths as provenance; they are not runnable after relocation.
Full historical capture records and review evidence remain in [Git history](https://github.com/khansen/xorcyst/tree/9a273fa081524e3d55d608b3c9587ad55261aca4/design/v2).

The 15 required ROMs are dpadhero2, ntxdev, bombsweeper and the twelve music
projects named in `baselines.json`. The frozen local edits are intentional; do
not replace them with clean Git checkouts. The original dpadhero project remains
non-blocking because it needs seven precompiled objects; no old-format reader,
object conversion or song recovery is required.

All converter outputs are frozen inputs. Wishmaster's `song2.inc` varied with
the environment despite identical converter inputs. Gate builds disable every
converter recipe and install failing converter stubs. Do not regenerate songs
inside an assembler/linker comparison. No `.o` or ROM is an input to the gate.
Both initial verification builds used different HOME/TMPDIR/tool paths and a
minimal environment. Tools recheck input hashes after building.

The Makefile target is not always the linker product. In particular,
there-must-be-an-angel targets `WelcomeToParadise.nes` but produces
`ThereMustBeAnAngel.nes`. DPadHero3Proto produces `DPadHero3Proto.nes`; the dated
leftover ROM is excluded. The gate verifies the actual product, not just make's
exit status. dpadhero2's implicit `reset` warnings are part of the baseline.

## Rebuild and measure

From the repository root, use new output directories:

```sh
python3 -B tests/projects/check_object_projects.py \
  --gate /absolute/archive/2026-10-22-frozen-assets-02 \
  --assembler /absolute/build/xasm --linker /absolute/build/xlnk \
  --output /absolute/new/candidate-builds
```

Run the same command with matched baseline tools into a second directory, then:

```sh
python3 -B tests/projects/measure_object_projects.py \
  --baseline /absolute/baseline-builds --candidate /absolute/candidate-builds \
  --output /absolute/new/object-survey --survey
```

`--project` selects a fixture for a focused check; the full compatibility gate
requires all 15. The default formal measurement uses 15 pairs and two-second
batches; `--survey` is a bounded five-pair absolute-cost survey, not a formal pass.
See the [performance policy](../../doc/performance.md) for sampling, memory,
artifact sizes and the explicitly accepted small costs.

`measure_analysis.py` measures Metroid/Zelda producers and the eight bundle
profiles. Supply `--capture`, `--baseline`, `--candidate`, `--consumer` (the paired
Nesrev `analysis_bundle.py`) and a new `--output`. Add `--bundles` for separate
producer, reader and complete-bundle measurements; `--mode` selects a mode or
profile. Defaults are a five-pair survey; formal sampling uses
`--pairs 15 --batch-seconds 2`. It verifies every PRG, but does not replace
semantic product-parity checks. The baseline path is specifically a 1.x tool;
the candidate uses the current CLI without a listing-version selector.

`../perf_ir_frontend.py` is the narrower plain-assembly harness; its `--fixtures`
argument selects the capture directory. `freeze_baselines.py` captures original
source/converter provenance into a new archive. `freeze_gate_assets.py` freezes
and verifies converter products from that capture. They are retained for
reproducibility, not run automatically or used to overwrite the established gate.

## Analysis compatibility inventory

Metroid and Zelda each emit 131,072 PRG bytes. Their source and reference hashes
are in `baselines.json`. Exercise all eight bundle profiles: ci-data-v1,
ci-instructions-v1, inventory-instructions-v1, process-instructions-v1,
maturity-instructions-v1, instructions-v1, data-listing-v1 and
pass-prep-instructions-v1. Keep the independent producer modes and alternate
formats too; one bundle does not exercise every CLI option.

The following inventory records the compatibility obligations established for
2.0. The listing-2 migration is complete; it remains a regression surface.

| Output / current consumers | Compatibility check |
| --- | --- |
| Structured listing — analysis_bundle, data_directive_xref, consumer_audit, orphan_opcode_scan, static_analysis, embedded_pointer_audit, pointer_table_body_check.py | Require listing 2; check corrected fill/non-emitting rows and inclusive ranges, emitted bytes and address/offset joins. JSON/NDJSON remain. |
| Xref — analysis_bundle, used_by_xref_check, branch_literals, split_pointer_targets, ram_accesses, pointer_targets | Keep version 2 facts/options, member exclusions and deterministic order. Check symbols, definitions, references, data edges and scope/filter behavior. Text/CSV remain. |
| Instruction records — instruction_records and its callers | Keep version 3 file table, byte validation, operand values, terms, trees, bindings and access classifications. Standalone and embedded representations agree. |
| Summary — pass-prep and ranking readers | Keep filters, rankings, referrers and nearby-window meanings in existing text/JSON/NDJSON forms. |
| Index patterns — embedded/pointer/table readers, project wrappers | Keep displacement/bounds/split-pair facts and their source and symbol joins. |
| Data consumers / coverage — consumer_audit, extent/blob-disposition readers, pass-prep | Keep span/access aggregates and uncertainty; distinguish inclusive listing ends from existing exclusive coverage ends. |
| Compare / raw-address audit — verify, project_compare, project_audit, project_pass_prep | Keep slices, source-mapped mismatches, ranges, levels and output/exit policy. |
| Dependency manifest — analysis_bundle and cache users | Keep input/negative-lookup identity, completeness, producer hash and manifest-last publication; changed producer invalidates caches. |
| FCEUX — current export consumers | Keep RAM/ROM NL format, output-offset PRG pages, mirroring and collision protection. |
| Object structured listing/xref | Explicit unsupported-mode error; no current game/Nesrev consumer. No linker analysis product. |

The frozen Metroid listing has null offsets in 6,951 of 50,834 rows. Preserve
those source/extent rows, validate emitted bytes against the binary, and test
both emitting and non-emitting rows in consumers, including Devil World's
`actor_render.render_sequences`. Project wrappers must use the current CLI
without `--listing-version`. No object-mode analysis or linker analysis product
is implied by this inventory.
