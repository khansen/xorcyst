# Consumed-input dependency manifest

Status: version 1 producer implementation. This is an input manifest, not a
validated downstream analysis bundle or a persistent cache.

## Invocation

```sh
xasm --pure-binary --dependency-manifest=inputs.json \
  --xref=analysis.json --xref-instructions=true input.asm -o output.bin
```

The option is absent by default. Version 1 requires pure-binary output and,
when xref is requested, JSON xref. An empty path or unsupported mode is CLI
exit 2. Producer identification currently supports macOS and Linux; other
platforms refuse this option rather than guessing the running executable.

The manifest is published only after assembly and all requested analysis,
audit, and comparison operations report success. Ordinary assembly failures retain
their failure status; dependency/snapshot/publication failures exit 3. Existing
outputs, including a previous manifest, can remain after failure. Callers must
discard evidence from an unsuccessful invocation, not infer success from a
pathname's existence. Publication uses a private sibling temporary file and
rename; it does not promise crash-durable filesystem transactions.

## Input acquisition and consistency

The producer hooks its real input open paths, without interpreting include
directives in a second parser. Each first successful open reads a regular file
into an invocation-local immutable snapshot and hashes those exact bytes. The
parser, binary-include loader, character-map loader, and later source readers
consume independent streams of that snapshot. Reopens use the same snapshot.
This prevents the manifest from describing one read while assembly consumes
different bytes from another read. No snapshot is reused across invocations.

Tracked roles are:

| Role | Input path |
|---|---|
| `source` | Root source and source includes opened by the lexer/resolver. |
| `binary` | Binary includes opened by the same resolver. |
| `charmap` | Character maps read during the semantic pass. |
| `analysis_source` | Additional source reads for listings, audit suppression, warnings, or comparison context. |
| `comparison` | The reference input to `--compare`, not the generated output being compared. |
| `producer` | The running executable identified using the operating system, not a PATH search or a trust in `argv[0]`. |

One pathname can have multiple roles. Source and binary includes are currently
opened during parsing even inside branches later removed as inactive; those
reads are dependencies too. Character maps retain their existing working-
directory resolution, not source-include search semantics. Existing diagnostic
source lookups retain their behavior; this option does not repair their source
attribution or change analysis policy.

Failed `ENOENT`/`ENOTDIR` probes are recorded as `missing_paths`, including
higher-priority include candidates examined before the selected file. These
negative dependencies matter: adding a previously absent candidate can change
future resolution even if every selected file's contents remain unchanged.
Paths after the successful include candidate are not probed or recorded.
Other read failures and nonregular inputs refuse certification. Named pipes
are refused without blocking in dependency mode.

Before publication, every consumed file (including the executable) is reopened
and compared by content hash and size, and every missing probe must still be
absent. Validation is repeated after writing the temporary manifest. Same-size
edits with preserved timestamps, deletions, changed symlink targets, and newly
present lookup candidates therefore invalidate evidence. Inputs are immutable
snapshots, but the filesystem as a whole is not locked: this is not a claim to
detect every transient change restored before validation or changes made after
the last check. Downstream reuse must validate again.

Source, comparison, and binary payloads are byte-oriented. Manifest path and
argument strings must be UTF-8; unsupported strings fail explicitly. Paths are
absolute lookup paths, not `realpath` identities: symlink and `..` components
are retained so revalidation follows the same path. Hardlink/alternate-path
aliases may produce separate entries. The instruction-record source-text
encoding contract is separate and unchanged.

Manifest paths must not alias an input, a lookup probe, or another output.
Registered output paths are also checked against known inputs and each other.
Before opening outputs, the producer conservatively reserves the final AST's
source-location lookup paths, including paths different from those selected by
include resolution. This prevents an output from truncating a file that a later
diagnostic reader would consume. A reservation alone is not a consumed or
missing dependency in the manifest; only actual reads/probes become entries.
These checks prevent ordinary accidental overwrites; they do not secure a
directory being maliciously mutated between filesystem operations.

## Schema

```json
{
  "schema": "xasm-dependencies",
  "version": "1",
  "producer_version": "xasm 1.6.1",
  "invocation": {"cwd": "/work", "argv": ["/tools/xasm", "--pure-binary", "input.asm", "--dependency-manifest=inputs.json"]},
  "inputs": [{"path": "/work/input.asm", "size": 18, "sha256": "<64 lowercase hex digits>", "roles": ["source"]}],
  "missing_paths": []
}
```

The example abbreviates the input list and digest; a real manifest includes
exactly one `producer` entry. Lists retain first-encounter order and role names
have fixed order. There is no timestamp or cross-build stable origin ID.

`argv` is captured before option parsing mutates defines or reorders arguments.
It preserves spelling, order, include paths, defines, and output selections.
Together with `cwd` and the executable digest it identifies effective behavior,
including defaults from that producer build. It is not a normalized options
map: semantically equivalent invocations can differ. The human-readable version
string alone is insufficient to identify a build. Executable hashing is an
execution identity check, not a promise of reproducible compiler outputs or a
cryptographic attestation of the running process.

SHA-256 follows [FIPS 180-4](https://doi.org/10.6028/NIST.FIPS.180-4). Tests compare
against independent library digests, including standard vectors, byte values,
padding boundaries, different update chunks, and a million-byte input. No FIPS
validation or security-certification claim is made.

## Downstream bundle boundary

This manifest does not hash output artifacts, add NESrev configuration/policy
identity, validate artifact schemas/completeness, or certify future reuse. The
wrapper still must bind the successful producer invocation to output hashes,
the requested artifact/version/options set, project/address/bank configuration,
and authored policy inputs. It must validate the manifest's consumed and missing
paths during production and reuse, and publish a completed descriptor only
after successful, consistent validation.

Missing, malformed, incompatible, partial, stale, or mismatched supplied bundles
must refuse, never silently fall back to assembly. The no-bundle standalone
mode remains a separately tested fresh-production path. No NESrev gate switches
until that boundary exists; see the [instruction producer contract](XASM_INSTRUCTION_RECORDS_SPEC.md#shared-analysis-bundle-boundary).

## Verification

`python3 tests/test_dependencies.py ./xasm` checks acquisition, exact digests,
argument preservation, include-search probes, byte/warning/xref parity,
collision/error refusal, and atomic manifest publication. A small compiled
driver pauses after snapshot acquisition so tests can independently mutate one
input condition before publication without timing races or shipped test hooks.
The unchanged-input control must succeed; changed-input cases must fail with
the dependency diagnostic, while reading the already-open snapshot still yields
the original bytes. `sh tests/regression.sh` runs this suite alongside the
instruction-record and existing assembler regression tests.
