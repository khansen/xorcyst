# FCEUX .nl symbol export

Status: implemented with automated exporter, compatibility, and allocation-failure
coverage in `tests/test_fceux_nl.py` and `tests/test_fceux_nl_alloc.c`, run by
`tests/regression.sh`.

## Purpose and invocation

Export names for FCEUX's debugger and trace logger from the same resolved program
facts used by xasm's analysis outputs.

```sh
xasm --pure-binary \
  --fceux-nl-rom-prefix=game.nes. \
  --fceux-nl-ram-output=game.nes.ram.nl \
  input.asm -o game.prg
```

- `--fceux-nl-rom-prefix=PREFIX` writes `PREFIX<bank>.nl`, with uppercase
  hexadecimal bank numbers: `0`, `1`, …, `9`, `A`, …, `10`.
- `--fceux-nl-ram-output=FILE` writes a single RAM/register name file.
- `--fceux-nl-mirror-16k` adds the other CPU window's address for each ROM label.
  It requires the ROM prefix and exactly 16,384 emitted PRG bytes.

Both output flags require `--pure-binary`. Empty paths and mirroring without a
ROM prefix are CLI errors. The flags default off and require neither `--xref`
nor its local/anonymous scope switches. Either output can be requested alone.
They can also accompany listings, xref, instruction records, and a dependency
manifest without changing the emitted binary.

## Physical ROM layout contract

The assembled binary must be the **raw PRG payload in physical ROM order**,
starting at PRG offset zero. It must not include an iNES header, trainer, CHR
payload, or a wrapper added by a later build step. The ROM prefix identifies
the files to write; xasm does not open the corresponding `.nes` file or infer
its mapper. Emitted code-segment bytes outside `$8000-$FFFF` cause a diagnostic
when ROM export is requested. This catches, for example, a header emitted at
`.ORG 0`; it is not a substitute for supplying the correct raw PRG payload.

FCEUX's default debugger bank is a physical **16 KB PRG page**, independently of
the mapper's hardware window size. Its bank filenames are hexadecimal.
Sources: [FCEUX debugger documentation](https://fceux.com/web/help/Debugger.html),
[NL file format](https://fceux.com/web/help/NLFilesFormat.html), and
[`getBank()` in FCEUX](https://github.com/TASEmulators/fceux/blob/master/src/debug.cpp).
FCEUX builds configured with a different debugger page size are outside this
version's contract.

For a label naming an emitted ROM byte:

```
bank        = output_offset / 0x4000
NL address  = resolved CPU address
bank count  = ceil(emitted PRG byte count / 0x4000)
```

The two coordinates are deliberately separate. `.ORG` changes CPU addresses;
it does not add padding to `--pure-binary` output. Several `.ORG` segments can
share one physical page, and one segment can span several pages. A 32 KB NROM
image produces two files. Four 8 KB CPU windows still occupy two FCEUX pages;
there is no special `$C000` split or mapper-specific inference in the exporter.

Every physical page produces a file, even if it contains no labels. Unlabeled
pages therefore cannot shift later page numbers. An empty PRG payload produces
no ROM bank files.

Segment extents are half-open ranges of **emitted bytes**. A label immediately
at a segment's end, with no following byte in that segment, is omitted from
ROM export. It can still be used in assembly and xref, but does not identify a
ROM byte. An empty segment's labels are likewise omitted. A label exactly at
`$C000` followed by a byte is included according to that byte's physical offset;
no label strictly beyond `$C000` is needed. `$10000` is not wrapped to `$0000`.
Dataseg declarations do not consume output bytes or establish ROM banks.

The mapping records the CPU addresses declared by the program. It does not
predict other runtime mapper configurations or invent additional CPU aliases.
A partial PRG fragment cannot acquire its full-ROM offset from its CPU origin;
assemble the complete payload before exporting.

## Names, aliases, and mirroring

Each line has the form `$ADDR#Name#Comment`. Entries sort by physical page,
CPU address, then **display name**. At a shared address, the first name becomes
the label and distinct remaining names appear as `aka Name2, Name3` in the
comment. Repeated identical `(page, address, name)` observations are collapsed.
A repeated name at a different address remains a separate entry.

Global labels retain their names. Local labels such as `@@Loop#42` use the
owning global label and the source spelling: `Main@@Loop`. A local before the
first global label uses `@@Loop`. Compiler namespace suffixes are removed,
including those introduced by macro expansion. Anonymous branch labels are
excluded regardless of `--xref-include-anon`.

Names and output paths use dynamically sized storage. No name is truncated to
fit a fixed buffer. A name containing an NL delimiter or a line break is an
explicit export error rather than malformed output.

For a 16 KB NROM image, mirroring adds the same name at `cpu_address ^ $4000`,
in physical bank zero. This works for source addressed at either `$8000` or
`$C000`, and includes local labels. Besides checking the byte count, the
exporter checks that each segment's CPU offset within the 16 KB window equals
its physical PRG offset. Merely finding one labeled bank is insufficient to
justify mirroring.

## RAM and register names

RAM output includes resolved dataseg labels in `[0, $8000)` and named operands
of direct memory instructions whose resolved base addresses lie in that range.
This includes reads, writes, and read-modify-write operations. Indexed absolute
and zero-page modes name the base address; FCEUX renders the index register.

An `.EQU` value alone is not evidence that it represents an address. Constants
used only as immediate values, arithmetic metadata, or branch/call targets are
not exported. Literal-only memory operands and expressions containing anonymous
labels are also excluded, including anonymous labels in macros. `JMP` and `JSR`
are excluded even though they use direct addressing modes. Indirect modes
(`(zp,X)`, `(zp),Y`, and indirect `JMP`) are not included in this version.

The existing instruction-provenance hook retains an operand AST before
constant substitution. RAM collection pairs that expression with the final
resolved operand value from the assembled instruction. It does not re-resolve
raw source names against the symbol table or use a constant's final definition
as the address of an earlier use.

The shared expression renderer produces a normalized display name from that
preserved AST. For example, `RAM_Base + FIELD, X` becomes `RAM_Base+FIELD`.
Integers use decimal spelling and nested parentheses preserve expression
structure. Every local identifier in a compound expression is qualified, so
`1+@@Buffer` becomes `1+Proc@@Buffer`. Macro arguments use their expanded
expression, and multiple instructions on the same source line retain their
own operands. String and datatype expression leaves are supported; string
control characters and `#` are escaped in display names.

These are output records representing observed memory addresses, not new
assembler symbols. Sorting and grouping handle repeated uses and aliases in
one place, including overlap with real dataseg labels.

## Internal design

The implementation separates collection, projection, destination validation,
and serialization:

1. **Collect program facts.** The shared `collect_analysis()` AST walk records
   all real label definitions and their CPU/output coordinates. An indexed
   name lookup updates definitions and resolves analysis references. Index
   values are array indexes, not pointers invalidated by `realloc`; the index
   is rebuilt after the symbol array is sorted. Symbol insertion and definition
   updates allocate all required fields before publishing the change, so a
   failed allocation cannot expose an incomplete entry. Local owners borrow the
   stable name of their real owning symbol, without duplicating the string.
2. **Project output views.** Xref's scope controls filter its serialization
   and address/name resolution. They do not discard real definitions from
   the shared collection. NL projection includes named locals independently.
   The same byte-advance helper handles instructions, data, storage, and
   binary includes and records segment extents when ROM layout is needed.
   RAM operands directly produce `(bank, address, name)` output records;
   `prepare_analysis_outputs()` adds ROM labels and mirror addresses after
   collection and validates names for a successful assembly. Diagnostic
   listings need destination planning without this projection step.
3. **Plan and validate destinations.** `plan_analysis_outputs()` expands exact
   filenames, including both CSV files and all RAM/ROM NL files, into an owned
   output plan. Invocation-wide validation checks these alongside the binary,
   its staging path, listing, and other analysis destinations. Collection and
   planning do not open output files.
4. **Serialize the collected results.** `write_analysis_outputs()` writes xref,
   instruction records, and NL to the validated destinations. `fceux_nl.c` owns
   the small NL output-record table, ordering, duplicate grouping, filename
   formatting, and file replacement. Its writer consumes the planned paths;
   it does not register destinations. It has no knowledge of assembler scopes,
   ASTs, equates, or mapper heuristics.

The caller supplies typed collection and output options, owns the collected
facts and output plan separately, and releases both explicitly.

Xref's indirect-flow analysis builds an address index of visible definitions
on its first eligible pointer read. Exact section/segment matches and the
fallback across sections preserve the existing symbol ordering for aliases.
Hidden definitions are filtered once when constructing the index. Each later
address lookup takes `O(log visible_symbols)` time.

There is no shadow local-label collection, synthesized `xref_symbol` array,
FCEUX-only symbol flag, or repeated search for `(segment, side)` bank keys.
RAM collection performs no scan of the symbol table. Symbol lookup has expected
constant cost; output sorting takes `O(entries log entries)` and emission is
linear in entries plus physical pages.

When NL is unused, its records and output extents are not allocated. ROM-only
export does not install instruction-provenance capture. RAM-only export uses
that existing hook without forcing full instruction-record construction.
Shared label facts and the name index are available to existing analyses.

Unused labels must survive assembly whenever an NL output is requested, just
as they do for xref/listing output. Retaining them does not suppress the normal
unused-label warnings or alter binary bytes.

## Failures and output lifecycle

Allocation failure is a failed operation with a nonzero assembler exit status;
it cannot be confused with an empty candidate list. Candidate ownership is
explicit, including ownership on failure, and cleanup releases partial results.

Input/output alias protection runs for NL exports even without a dependency
manifest. It reuses the dependency subsystem's path tracking without enabling
content snapshots. With a manifest, the normal snapshot and provenance checks
continue to apply. Every destination is checked before any output is opened,
including the binary staging path, both expanded CSV paths, instruction records,
RAM/ROM NL files, listing, and other analysis outputs. The manifest reserves its
own destination at startup. A collision leaves all existing outputs intact.

Diagnostic listings also use this validation after assembly errors, including
reservations for the comparison reference, binary, expanded CSV files and NL
files. Name projection is skipped on those failed builds. If layout collection
cannot resolve a required storage size, it leaves outputs untouched.
An implicit binary filename is reserved for validation without changing the
diagnostic listing's output metadata; an explicitly requested filename remains
in that metadata.

Path comparison follows dangling symlink chains as well as existing files and
symlinked directories. On macOS it queries filesystem case sensitivity and uses
the system CoreFoundation Unicode comparison facilities for future filenames,
including case and normalization aliases. The macOS build links that system
framework; configure checks its headers and linkage without executing target
code, and the framework supplements project-wide link additions. Other builds
retain POSIX path comparison. An indeterminate case-sensitivity query uses
case-insensitive comparison conservatively; query errors still stop publication.
Existing binary staging
paths must be regular files; symlinks, FIFOs, directories, and sockets are
rejected before publication. The staging open uses `O_NOFOLLOW | O_NONBLOCK`
and checks the opened descriptor before truncating or writing. A failed stream
initialization closes the descriptor and removes the regular staging file.
Exclusive creation records staging-file ownership. A failed descriptor check
removes a file created by this invocation and preserves a pre-existing node.

The binary and each sidecar are written to temporary files beside their
destinations. Publication requires a clear stream error indicator and a
successful close, followed by a direct rename over the destination. A write,
close, or rename failure leaves that destination's previous contents intact
and removes the temporary file. Binary failures prevent analysis and manifest
publication; diagnostic listings may still describe the failed build.
The entire set of output files is not an atomic transaction: a later filesystem
error may follow successful replacement of an earlier file. The shared sidecar
writer covers listings, all xref formats, standalone instruction records,
summaries, data analyses, NL files, and manifests. CSV closes both streams
before either file is published. The manifest still revalidates consumed
inputs after closing its staged stream and before publishing it.

Sidecar stages use a fixed-length `.xasm-XXXXXX` basename in the destination
directory, so long destination basenames remain supported. Only stages created
by this invocation are removed. Publication replaces a symlink itself and
rejects FIFOs, sockets, devices, and directories. After successful serialization,
the writer preserves an existing regular destination's read/write/execute bits
(following a destination symlink for those bits when available), or applies
mode 0666 filtered by the process umask for a new file or an uninspectable
symlink target. This permission fallback does not weaken NL/manifest alias
validation: unresolvable output paths still fail before any publication.
Failure to set these permissions discards
the stage and preserves the destination. Analyses that select stdout check
stream/flush errors without closing stdout.

JSON xref owner, address, and data-flow analysis completes before its destination
is opened, so those allocation failures preserve existing xref contents. Symbol
enumeration returns an explicit failure and an empty list on allocation failure;
only initialized names are freed, and callers propagate failure.

The exporter does **not** delete files for higher bank numbers left from an
earlier invocation. A matching filename alone does not establish ownership;
such a file could contain hand-written symbols or be another build input.
The build process should manage obsolete sidecars in its output directory.
This replaces the preliminary branch's unbounded, unchecked stale-file deletion.

## Validation

`tests/test_fceux_nl.py` has 43 tests, with matrices covering xref formats,
local/anonymous filters, and analysis outputs. It covers physical page numbering, unlabeled pages,
local-only pages, the exact `$C000` boundary, end labels, multiple origins per
page, four-window layouts, hexadecimal filenames beyond bank 9, both NROM
mirror directions, byte-count validation, dataseg separation, expression names,
macro expansion, same-line instructions, long names, alias grouping, xref
formats, instruction records, path collisions, and failed writes. Collision
tests preserve sentinel contents in every destination, with and without a
manifest, including expanded CSV paths and an aliased binary staging path.
They also exercise early diagnostic listings, comparison-reference protection,
dangling staging/listing links, and filesystem-equivalent future filenames.
Anonymous memory operands, including expressions mixing named and anonymous
symbols, are exercised inside and outside macros with xref's anonymous-label
switch enabled and disabled. NL contents
must be identical with xref absent or configured differently. Listings (text,
JSON, NDJSON), summaries (text, JSON, NDJSON), index patterns, data consumers,
and data coverage must retain their contents when NL is enabled. Tests remove
previous outputs before comparisons that require a freshly generated file.
Standalone instruction records remain byte-identical, and dependency manifests
retain their contents apart from the expected invocation argument changes.

`tests/test_fceux_nl_alloc.c` injects failures at the 79 allocations reached in
the NL table, destination planning, writer, and fixture-name allocations.
Each injected failure must return failure.

`tests/test_build_configuration.py` checks generated build rules in an isolated
source copy: both programs retain global link additions, and Darwin configure
rejects missing CoreFoundation headers or an unavailable framework. The
dependency suite injects known, indeterminate, and failed case-sensitivity
queries, verifying that unknown capabilities still reject case/Unicode aliases
while permitting distinct names.

`tests/test_output_failures.py` builds the real assembler with test-only wrappers
around its CLI, shared output writer, analysis, and symbol-table translation units.
Its tests inject
476 allocations during shared collection (including symbol/index and extent
growth and long local RAM expressions), three during shared destination
planning, 108 during NL name projection, and the visible-address-index allocation.
Four constant-enumeration allocations and 17 xref owner/data-flow allocations
are each failed with NL disabled and enabled. These cases verify both safe
cleanup and preservation of existing xref and NL destinations.
Every injected allocation failure must produce a nonzero exit without a crash;
failures before publication must preserve all existing destinations. This does
not cover allocations in parsing, AST evaluation, or path validation.

The output matrix fails allocation, creation, stream initialization, stream
status, flush, permission setting, close, and rename for every sidecar format
with existing and absent destinations. It verifies preservation of the failed destination and cleanup
of owned stages, including both CSV files, diagnostic listings, instruction
serialization failures, and stdout analysis errors. Long filenames, unowned
stage-like filenames, and non-regular destinations are covered separately.
Permission tests cover existing modes 0600, 0644, 0664, and 0755 and new outputs
under umasks 0002, 0022, and 0077 for every sidecar format, including both CSV
files, NL banks, and manifests. Symlink replacements also cover existing and
missing targets without modifying the target. Inaccessible and cyclic targets
use the umask when replacing a listing symlink without NL/manifest protection;
the same paths are rejected before any writes when that protection is enabled.
Manifest I/O failures emit one diagnostic while retaining the failure status.

The same harness injects I/O failures during temporary-file creation
and subsequent output operations.
Binary `fstat` and `fdopen` failures cover fresh and existing staging files.
Binary `ferror`, `fclose`, and `rename` failures cover fresh and existing destinations, raw binary
output with NL enabled and disabled, and object output. Successful replacement
is also checked in all three modes. Each NL `mkstemp`, `fdopen`, `ferror`,
`fflush`, `fchmod`, `fclose`, and `rename` operation is failed at the RAM file and
both ROM banks, checking temporary-file cleanup, preservation of the failed and later
destinations, and the documented retention of files already published.
`TEST_FAULT_CFLAGS` can add sanitizers to this build;
normal exporter and address-view tests are also checked with AddressSanitizer
and UndefinedBehaviorSanitizer.

`tests/verify_fceux_projects.py` provides opt-in checks using larger, external
projects and a pre-feature executable. It compares binary bytes, diagnostics,
and analysis artifacts with NL both disabled and enabled in seven modes:
plain assembly, xref, xref with locals/anonymous labels, xref with data/owner
analysis and instruction records, listing, summary, and the three other
analyses together. Only build timestamps are excluded from JSON comparisons.
It also checks identical NL contents across these modes and compares PRG
bytes against an adjacent reference ROM when one is available.

Build both executables with identical compiler flags, then run:

```sh
python3 tests/verify_fceux_projects.py \
  --baseline /tmp/xasm-before-feature --candidate ./xasm \
  --source /path/to/donkey_kong/asm/donkey_kong.asm \
  --source /path/to/kid_icarus/asm/kid_icarus.asm \
  --report /tmp/fceux-acceptance.json
```

After compatibility checks, this script measures plain assembly, xref, full
xref, listing, and summary **without NL**. It warms both executables and
alternates their order for paired samples. The JSON report contains executable
hashes, every wall/CPU sample, medians, and a configurable CPU regression
threshold (default: 20% plus 20 ms). Timings run sequentially; increase
`--repeats` when process timings vary substantially.

Add `--hidden-locals` to exercise 1,000, 4,000 and 8,000 hidden local definitions
and indirect reads with NL disabled. This compares diagnostics, binary and
xref contents against the baseline, and applies the same paired CPU threshold.
The fixture specifically covers the repeated address-lookup cost that the
external game projects did not expose.

Donkey Kong (16 KB) and Kid Icarus (128 KB) pass the compatibility matrix
against commit `00617b8d69a9e8a78c4bada5459c5cb0df6e3397`, before this feature
was introduced. Both match their reference PRG payloads byte-for-byte. Kid
Icarus produces eight physical page files; Donkey Kong produces one with both
CPU mirror windows named. All project inputs and deployed sidecars remain
untouched; generated files use temporary directories.

The recovery audit built that baseline and the candidate with Apple clang
21.0.0 and identical `-DHAVE_CONFIG_H -g -O2 -Wall` options. Median CPU time
ratios with NL disabled were:

| Project | Plain | Xref | Full xref | Listing | Summary |
| --- | ---: | ---: | ---: | ---: | ---: |
| Donkey Kong | 0.84 | 0.87 | 0.91 | 0.93 | 0.88 |
| Kid Icarus | 0.75 | 0.91 | 0.76 | 1.01 | 0.76 |

Ratios are candidate/baseline, using five paired samples per mode, except Kid
Icarus listing, which uses all 20 pairs from the initial run and a longer
repeat. Its initial five-pair result was 1.31 and triggered investigation;
the next 15 pairs measured 0.88. Temporary phase instrumentation placed the
variation in parsing, with median listing generation itself at 0.160 seconds
before and 0.159 seconds after the feature. These measurements establish no
substantial slowdown in the exercised modes; the wide timing variation is
also why they should not be treated as precise speedup claims.

The destination-planning and anonymous-operand fixes were checked against the
preceding feature executable (`d90936d`) with the same seven-mode compatibility
matrix on both projects. Five paired samples of plain assembly and full xref,
with NL disabled, also remained within the existing timing threshold in all
four project/mode combinations.

A subsequent review exposed quadratic visible-address lookup work that those
projects did not exercise. The hidden-local fixture is now part of the opt-in
verification script. In five paired runs, total CPU ratios against master were
1.08, 1.04 and 0.98 for 1,000, 4,000 and 8,000 locals/reads respectively, with
identical outputs and diagnostics. Temporary phase instrumentation measured
collection plus serialization at 0.80, 2.07 and 3.99 ms after indexing; the
preceding feature build measured 4.98, 66.41 and 252.10 ms. These phase timings
isolate the addressed regression from the assembler's other costs.

After staging and allocation-failure hardening, all 22 configured NESrev projects
pass the seven-mode matrix against the same master baseline with NL off/on.
Every PRG matches its configured reference, warning baselines match, and the
66 regenerated NL files match the previously installed files byte-for-byte.
This corpus includes eighteen mirrored 16 KB NROM programs, one 32 KB NROM
program, and three 128 KB MMC1 programs. Five paired hidden-local runs after
these fixes measured total CPU ratios of 1.03, 1.00 and 1.02 for 1,000, 4,000
and 8,000 locals/reads, within the existing acceptance threshold.
