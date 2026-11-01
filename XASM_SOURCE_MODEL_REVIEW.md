# Review request: xasm lossless source model specification

Review [XASM_SOURCE_MODEL_SPEC.md](XASM_SOURCE_MODEL_SPEC.md) as a production
contract proposal. This is a documentation-only branch based on
`51dd295b89073b7858e4c6cf9a19776af7e9bbc7`; it does not implement the contract.
The local spike is feasibility evidence, not an API that must be preserved.

The goal is reliable structured inspection/editing using the assembler's own
facts, with a byte-exact source -> saved model -> source no-op round trip when
the original source is unavailable. Storage speed and token savings are not the
justification. Consumer services enforce access/edit/review policy; xasm supplies
complete evidence for the supported profile and refuses incomplete certification.

The owner permits breaking changes, but any public break requires xasm 3.0.0.
Evaluate the compatibility ledger explicitly. Do not require legacy adapters to
avoid a major release, and do not infer that instruction schema 3 means xasm v3.

Focus on:

1. Whether exact bytes and complete typed coverage are both enforceable, including
   `.END`, `.BYTE`, anonymous/local labels and forced-absolute operands.
2. Whether every reference is captured from actual native resolution, including
   nested operands, data and EQUs; whether ranges come from native emission.
3. Whether spans, scope/segment IDs, evaluations and identities suffice for bound
   edits and reopened review without consumer source parsing or hidden files.
4. Whether the capability boundary matches the grammar and avoids either silent
   incompleteness or unnecessary include/macro/object implementation in v1.
5. Whether diagnostic models, errors, input checks and output publication can
   expose stale facts or falsely certify success after a partial failure.
6. Whether the proposed compatibility classification misses a real break, and
   whether keeping any existing output would force needless duplicate machinery.
7. Whether the acceptance tests prove the bad direction, and the cost measurements
   cover the work actually performed without implying an unmeasured speed budget.

Useful implementation references in this repository:

- `scanner.l`, `parser.y`: source handling, `.END`, include timing and parse recovery.
- `astproc.c`, `symtab.c`: binding, substitution, native scope and redefinition.
- `ir/frontend.c`, `ir/layout.c`, `listing.c`: provenance, layout, emission and
  existing instruction/data analysis; confirm actual paths rather than assuming
  a collector and the emitter use the same binding.
- [2.0 contracts](XORCYST_V2_CONTRACTS.md),
  [dependency snapshots](XASM_DEPENDENCY_MANIFEST_SPEC.md),
  [data references](XASM_DATA_DIRECTIVE_REFERENCES_SPEC.md),
  [instruction access facts](XASM_INSTRUCTION_RECORDS_V2_SPEC.md), and
  [current instruction wire format](XASM_INSTRUCTION_RECORDS_V3_SPEC.md).

Keep findings specific: requirement/location, concrete counterexample, impact,
and smallest correction. Distinguish missing contract details from defects in
the known disposable probe. Do not claim tests passed for an unimplemented API.
Do not run private corpus rebuilds or model sessions for this spec review; code
inspection and small neutral examples should establish design findings. Do not
modify the spike, retained evidence, project branch or authentication state.

Return APPROVE or REVISE with required findings and a short optional list. Review
approval is not release or push authorization. All remote actions await the owner.
