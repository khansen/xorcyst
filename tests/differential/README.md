# Generated-program differential

This optional maintainer check compares two XORcyst 2.x toolchains. The default
workload is **100 generated pure-binary programs, 100 generated multi-unit link
programs, and five focused controls**. Each binary program runs through 26
configurations covering the supported listing, xref, summary, index-pattern,
consumer, coverage, audit and comparison formats, standalone records and
dependency manifests, RAM/ROM debugger names, debug/case options and selected
filters. Comparison configurations exercise both exact matches (exit 0) and
deliberately changed reference bytes (exit 5), in text and JSON. The mirror
control also exercises a complete 16 KiB ROM.

The suite uses only the Python 3.9+ standard library and public synthetic inputs.
It needs no private projects, review scratchpads or external evidence archive.
The ordinary `make check` validates the harness with current tools and deliberate
mutants; it does **not** build or download a baseline.

## Run

Build the candidate normally, then from a Git checkout:

```sh
./configure --disable-maintainer-mode CFLAGS='-O2 -g -Werror'
make -j2
python3 -B tests/differential/run.py \
  --candidate-dir "$PWD" \
  --cache /absolute/reference-cache \
  --output /absolute/new/results
```

Or use `make check-differential DIFFERENTIAL_OUTPUT=/absolute/new/results`.
`DIFFERENTIAL_FLAGS` passes optional arguments to the runner.

The output directory must not exist. The runtime budget is 300 seconds, excluding
tool compilation; each tool invocation has a five-second timeout. The runner
marks a timed-out or unfinished workload incomplete and returns nonzero. It
never reduces the requested matrix to claim a pass. Exit 0 means all requested
comparisons and invariants passed; exit 1 means a difference, incomplete run or
execution failure; argparse uses exit 2 for invalid command-line arguments.

The default seed is `20261027`. Each case derives its own RNG from the seed, path
and index, so increasing the counts preserves earlier cases. Ten of the default
100 binary programs exercise reassignment chains; the other 90 use the broader
analysis generator. The link generator emits two to four units in randomized
order. All cases, including rejected programs, run through their entire matrix.

To compare already built tools, including from a source archive without Git:

```sh
python3 -B tests/differential/run.py \
  --baseline-dir /absolute/reference-tools \
  --candidate-dir /absolute/candidate-tools \
  --output /absolute/new/results
```

Each directory must contain `xasm` and `xlnk`. Tool paths, executable hashes,
versions, host, Python version, exact arguments and elapsed times are recorded.
Every invocation uses the same scratch pathname for the two tools, with
identical filenames and a minimal environment (`PATH`, `HOME`, `TMPDIR`, locale,
timezone). No old generator environment switches are consulted. Existing-output
and absent-output cases alternate, and every invocation resets its initial state.
The directory itself stays in place while its contents are cleared, avoiding
stale working-directory identities on shared filesystems such as Docker mounts.

For a longer opt-in campaign, specify `--binary-cases 1000 --link-cases 1000
--seed 42 --budget 1800`. The GitHub workflow's manual inputs expose case counts
and budget too. PR CI always uses the fixed default workload on Linux and macOS.
Manual CI accepts budgets up to 1800 seconds with a 45-minute job allowance for
builds, validation and artifact upload. Larger local campaigns remain available.

## Reference builds

`baseline.json` pins the actual 2.0.0 release commit,
`8e2257f89827d0a274105a19a76cac7ffc42d2e5`. `build_reference.py` exports that exact
commit from local Git history and builds the checked-in generated sources in a
fresh directory. It does not fetch, install tools, or change the working tree.
A shallow checkout needs the pinned commit available; CI uses complete history.

The build uses `CC` (default `cc`), `CFLAGS` (default `-O2 -g -Werror`), and the
recorded `CPPFLAGS`, `LDFLAGS`, `SDKROOT` and deployment-target settings. Use the
same compiler and settings for the candidate. CI does this explicitly. Cache
keys include source revision, compiler command/version, flags, host/architecture
and recipe hash. A receipt records the commands and executable checksums; cached
executables are checked before reuse. Build logs survive a failed compilation.
The default cache is under the current user's cache directory:
`~/Library/Caches/xorcyst/reference` on macOS, or
`${XDG_CACHE_HOME:-~/.cache}/xorcyst/reference` on Linux. Explicit `--cache` paths
must also be owned by the current user and protected from other users' writes.
New cache directories (including missing parents) use mode `0700`, tools use
`0755`, and receipts and build logs use `0644`, independent of the umask.
Existing directories are checked without changing their permissions.
Symlinked cache entries and unsafe writable parents are rejected. Checksums
detect corruption within this trusted directory; they do not authenticate an
arbitrary receipt supplied by someone else.

Retain the 2.0 pin for cumulative 2.x comparisons. Use explicit tool directories
for an immediate-parent comparison or secondary 1.9.12 compatibility research;
the default matrix expects 2.x CLI and schema contracts, so it is **not** an
automatic v1 compatibility oracle. Do not change the pin to conceal a regression.

## What is compared

Exit status, stdout, stderr (including warning counts and source locations),
artifact presence and contents are compared. JSON compares structurally while
preserving array order, value types, locations, nulls and missing fields. Binary,
CSV and text outputs compare byte for byte. Each version builds and links its
own objects; object bytes are retained and hashed, but only object presence,
assembly diagnostics and final linked bytes form the differential oracle.

Normalization is deliberately narrow:

- Validate `x.json`'s `build.timestamp_utc`, then replace that clock value.
- Validate the dependency manifest's producer version, executable path/size/hash
  and `argv[0]` against the actual invoked executable, then mark those identities.
- Ignore JSON formatting and object-key order. Preserve every other field,
  including output paths, input hashes, missing probes and row order.

Independent checks reconstruct the binary from structured listings, check
instruction bytes/operands and data-reference values against it, compare
embedded and standalone instruction records, and require requested artifacts.
Every input is hashed after each invocation, including unused auxiliary files;
changes fail the run and their hashes and bytes are retained. Accepted runs must
replace every seeded output, independently of agreement between toolchains.
Focused controls require known bytes, instruction counts, successful imported
strings and aliases, and rejection with preservation of a previous binary.
The ordinary suite injects changed bytes, diagnostics and missing artifacts and
checks that the runner saves failures, that replay reproduces them, and that the
reducer preserves all recorded failure classes. Empty listing rows cannot pass
by equality.

## Coverage and failure bundles

`report.json` (schema 2) contains planned, attempted, completed and timed-out cases
and invocations, acceptance disagreements, warnings and omitted configurations.
`products` separates artifact presence from fresh successful output, nonempty
bytes, structured record counts and runs containing zero records. Text formats
without a structured row parser report bytes only, not semantic coverage.
`configuration_rejection_reasons` groups rejection categories for every
configuration, including ROM export and warning-as-error modes; exact diagnostics
remain in each result. Categories count rejected case/configuration pairs that
contain that reason; multiple reasons can make their sum exceed the rejection
count. A fully executed configuration with no validated successes
on either side fails the coverage gate, even when both tools reject identically.
Must-accept controls already catch this where they apply; the coverage gate also
protects configurations without those controls.
The report separates
generated-program acceptance from focused controls. `source_features` counts
lexical feature presence and selected combinations in generated and accepted
programs, including categories never accepted. These are source-presence counts,
not branch or C-code coverage. Accepted-feature counts include programs accepted
in at least one configuration; the separate program counts use plain assembly
or final linking. An all-equal run can still have many rejections.

An initial report is written before baseline compilation. Atomic checkpoints
are refreshed every 100 pairs and on normal completion, exceptions, SIGINT and
SIGTERM. `results.jsonl` is flushed after each pair; `active.json` identifies the
pair in progress. After an uncatchable kill, the last checkpoint remains marked
incomplete and the journal can contain newer pairs. No final pass is inferred.

Exhausting the run budget is recorded separately from reaching an invocation's
own timeout. Budget cuts produce `incomplete/` evidence and omitted pairs, not
regression failures or tool-timeout counts. These bundles cannot be replayed as
recorded regressions. A genuine per-invocation timeout still fails the comparison
and retains a failure bundle.

Every generated source and auxiliary input remains under `cases/`. Unexpected
differences additionally produce self-contained `failures/CASE-CONFIG/` bundles
with input bytes, configuration, tool identities, exact commands, raw streams,
artifacts and structural difference locations. CI uploads results and reference
build receipts/logs even on failure. It retains them for 14 days; copy evidence
needed longer to durable storage.

Replay with the recorded executables (their hashes must still match):

```sh
python3 -B tests/differential/replay.py /absolute/results/failures/CASE-CONFIG \
  --output /absolute/new/replay
```

Add `--reduce` for bounded line deletion across source units and the linker
script. The original bundle is never modified; auxiliary files stay intact.
Replay must reproduce the complete set of recorded difference classes and both
tools' acceptance/exit patterns. The reducer preserves that entire signature,
so a warning difference cannot stand in for a missing byte difference. Structural
row-count magnitudes may shrink, but their paths and difference types remain.
Defaults are 200 attempts and 60 seconds; minimality is not guaranteed.
An explicit `--baseline-dir` or `--candidate-dir` permits replay with replacement
builds. Replay returns 1 when the recorded signature reproduces, 0 when there are
no differences, and 2 for a different signature or an incomplete replay. Its
report identifies missing and newly observed difference classes.

Classify a difference before changing code: regression, inherited defect,
harness defect, or proposed semantic change. Add minimized regressions to the
ordinary suite. No broad category or field should be excluded to hide a failure.
There is not yet an approved-differences allowlist. Intentional behavior changes
need a separately reviewed, narrowly scoped expectation mechanism; a version
bump alone does not require moving the baseline.

## Provenance and remaining integration

`provenance.json` records SHA-256 hashes and byte lengths of the preserved review
inputs used for this adaptation. The original archive remains unmodified. The
suite never reads it at runtime. The inherited v1 names and prototype executable
assumptions have been removed; `.char`, string data, relative `.org`, procedure
undefinition and forward strings are enabled. The random multi-unit generator
still excludes imported strings, covered instead by the focused link fixture.

| Preserved tool | Maintained replacement or remaining work |
| --- | --- |
| `adiff56.py`, earlier `adiff.py` | `run.py`, `common.py`, `products.py`; rejected cases retained and normalization audited |
| I5/I6 `agen.py` | `analysis_gen.py`; fixed seeds and no feature-disabling environment variables |
| `linki56.py`, earlier `linkdiff.py` | `link_gen.py` and the shared runner; normal tool names, same paths, own objects |
| `assigngen.py` | `assignment_gen.py`; ten default generated binary cases |
| `selfcheck.py` | `products.py` plus explicit positive and negative fixtures |
| `ddmin.py`, `jdiff.py` | `replay.py` and `common.structural_diff`; bounded reduction and typed differences |
| `injsweep.py`, `stagetest.py`, `diagasan.py`, `failalloc.c`, `failfs.c`, `injbt.sh` | Pending portable integration with existing allocation/output-failure tests; original DYLD and exact-path assumptions need separate validation |
| `objfuzz2.py` | Pending integration with `test_ir_objects.py`; the existing wire/allocation tests remain in ordinary and sanitizer CI |

The bounded suite does not replace fault injection, object mutation, the private
ROM gate or performance measurements. The broader integration TODO remains open
until the fault tools are ported and the fresh-host Linux/macOS workflow has been
validated. No runtime assembler or linker behavior is changed by this tooling.
