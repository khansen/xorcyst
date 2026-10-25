# I5 analysis integration

Status: source capture and structured listings implemented, local only. This is
an implementation checkpoint, not an
I5 approval or a replacement for the approved contracts. Base: I4 merge
`63747edf648aed016928c4e2671c7f4ad4236c37` (PR #100). PR and post-merge Ubuntu,
macOS and sanitizer jobs passed before work began.

## Source-capture prerequisite

Three existing implementation details prevented safe use of the collectors:

- `v2/frontend.c` reused `analysis_origin_id` for cached string values and
  captured character maps. An analysis ID could therefore be interpreted as
  an unrelated value/map ID.
- The prototype aliased the parsed source location and the expansion location.
  Cloning a macro lost the original source span needed by instruction records.
- The v2 return in `process_data` preceded the data-provenance hook, so the hook
  was never called for native v2 data statements.

The prerequisite separates the two ID domains, adds opt-in parsed source spans
to the same AST allocation, and runs the hook before frontend reduction. The
fixed AST node remains 136 bytes on this host. Capture adds 32 bytes per node
only when enabled. The source-span allocation bit belongs to the node; cloning
still drops transient binding and pending-guard flags. Existing v1 renderer
reads use an accessor that returns their original `source_loc` field.

Normal prototype invocations do not enable the rich capture yet. A test-only
driver enables capture before parsing and marks provenance
IDs while assembling real macro, string, forward-alias and charmap inputs. It
checks exact bytes and original-versus-expansion source lines. No wire-format,
language, installed CLI, or analysis schema changes are made here. This
prerequisite was saved as local commit `5af5325`.

## Structured listings

The prototype accepts pure-binary JSON and NDJSON listings using schema 2,
defaulting to it when no selector was supplied. Other analysis products and
text listings remain unsupported until their collectors have been ported.

Lowering retains a small owned row table: source location, statement kind,
label spelling/scope role and a half-open range of layout fragment IDs. Native
data retains its directive kind for analysis. Plain builds still compact it.
Literal runs flush at row boundaries only when rows are requested; non-debug
data and known storage retain the existing listing grouping in metadata.
Warnings still occur at their plain-build stage. No source-row entry borrows
an AST node or symbol-table entry, and no new value/layout engine is used.

After successful layout, the view reads final opcodes, positions, lengths and
bytes from the shared backend. Binary RAM facts now freeze their separate
address/extent, while contributing no output bytes. The compact and full fact
representations and streamed encoder are tested, including later graph edits.
The final row coverage is validated before binary publication. Assembly errors
preserve both the previous binary and listing instead of publishing a partial
listing with invented values. The existing destination checks protect inputs.

Current controls cover final narrowing, branches, code/RAM origins and segment
switches, fill spans, word/dword data and strings, empty output, forward mapped
field strings and padding, the relaxation cascade, wide binary PCs, includes,
incbin, debug grouping, warning parity and output preservation. JSON and NDJSON
rows are compared and reconstructed into the exact emitted binary.

Both frozen projects give byte-for-byte identical listings to v1 in both
formats: Metroid 50,834 rows; Zelda 45,699 rows. PRGs match their reference and
diagnostics match v1. A short five-pair survey (one warm pair discarded) gives:

| Project / format | v1 elapsed ms | candidate elapsed ms | v1 / candidate peak RSS bytes | Listing bytes |
| --- | ---: | ---: | ---: | ---: |
| Metroid JSON | 170.50 | 161.83 | 49,299,456 / 47,644,672 | 27,072,685 |
| Metroid NDJSON | 167.74 | 160.00 | 49,315,840 / 47,611,904 | 26,818,253 |
| Zelda JSON | 136.68 | 127.01 | 40,648,704 / 36,061,184 | 24,395,332 |
| Zelda NDJSON | 135.72 | 125.79 | 40,730,624 / 36,044,800 | 24,166,581 |

These are median spot measurements on one macOS arm64 host, not the formal
I5 producer/consumer/bundle gate. Raw samples and complete outputs are under
`/private/tmp/xorcyst-i5/listings/` (`spot-resources.json`).

Validation of this listing checkpoint:

- Frontend: 57 tests; layout: 32 tests, plain and macOS ASan/UBSan.
- Objects: 38 tests. The linker still emits exactly the same bytes in its
  regression suite; RAM facts do not enter the byte stream.
- `make check`: 545 tests in 19 suites plus shell regressions.
- `make distcheck`: 550 tests in 20 suites, archives ready.
- Targeted mutants are caught for missing RAM addresses/extents, emitting RAM
  in the streaming writer, and repeating the early data warnings at lowering.
  Logs are in `listing-mutants/`; full checks use `listing-*.log`.
- `git diff --check` is clean. The archive checks include the new controls.

One initial test mistakenly supplied an empty source file, which the existing
parser rejects. The zero-output control instead uses a valid equate-only unit.
No parser/language acceptance rule changed.

## Integration still to do

The approved product inventory and acceptance gates remain in design section 5
and contract section 6. This is the implementation order within I5:

1. Build the analysis input from owned source/binding metadata and the shared
   backend's final layout facts. `lower()` currently frees instruction/data
   statements; `xasm.c` then frees the namespace and remaining AST before
   layout. Keep that ownership boundary. Collectors must not keep borrowed
   parser statements or re-run the old expression/layout engine.
2. Connect listing rows, symbols, references, instruction operands and additive
   terms to that input. Addressing modes, bytes, offsets, lengths and values
   must describe the same successful layout that produced the binary. Cover
   forward strings, captured charmaps, assignment generations, macro/local
   bindings, null offsets, fill spans and fail-closed publication.
3. Port the remaining inventory: xref summary, index patterns, data consumers,
   coverage, raw-address audit, comparison, manifests and FCEUX outputs. Keep
   the approved pure-binary boundary for structured analysis.
4. Update Nesrev consumers and fixtures in an isolated checkout, then run the
   real profiles and producer/consumer/whole-bundle gates. Plain-project and
   object/link regression gates remain required. Report sizes, peak memory and
   absolute timings alongside ratios. The owner's bounded-effort guidance for
   small costs still applies; it does not waive large-project analysis costs.
5. Complete local independent review and owner approval before publishing I5.
   I6 remains the coordinated cutover and removal of superseded runtime paths;
   the temporary prototype names and build selections are not the final design.

At this checkpoint `listing.c`'s xref, instruction-record and derived-analysis
collectors still read the old AST/symbol table and have their own evaluators.
They are not enabled for v2. The v2 structured listing bypasses those paths;
the remaining dependencies are the next integration work, not an accepted
permanent fallback. Nesrev was
inspected read-only at `260a50bd79a6e4a8a3f4104a3b928aee40d02154`; its existing
local edits were left untouched.

## Validation of this prerequisite

- Normal strict `-Werror` build passes.
- Frontend suite: 53 tests, plain and macOS ASan/UBSan. Five new tests cover
  optional allocation, nested clone ownership, distinct IDs, allocation failure
  and the real-source hook.
- Object suite: 38 tests. Existing v1 instruction-record suite: 36 tests.
- `make check`: 540 tests in 19 suites and the shell regressions.
- `make distcheck`: 545 tests in 20 suites, archives ready.
- Two targeted mutants fail the real-source capture test: using the analysis
  ID as a string-cache ID, and losing the original source span when cloning.
- Frozen Metroid and Zelda each emit their exact 131,072-byte reference PRG
  with I4, the candidate, and the capture driver. Diagnostics match after
  removing the driver's test trace. Capture runs visit 16,767 and 7,556 native
  data statements respectively. Source hashes are unchanged. These are byte
  checks, not timing or memory gates (`foundation-projects.json`).
- `git diff --check` passes. The two new C harnesses are in the distribution.

The first sandboxed full runs could not bind the Unix socket used by existing
output-protection tests. Both complete checks passed when rerun with the
required sandbox permission. Whole-frontend `-Wconversion` is not claimed:
that attempt encounters existing conversions in `astproc.c`/`astnode.c`.

Evidence is under `/private/tmp/xorcyst-i5/` and
`/private/tmp/xorcyst-i5-*.log`. No Linux, 32-bit, big-endian, I5 consumer or
I5 performance-gate result is claimed by this checkpoint. Full I5 output and
consumer validation remains open.
