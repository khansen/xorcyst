# Instruction records version 3: shared file table

Status: implemented. Extends
[instruction records version 2](XASM_INSTRUCTION_RECORDS_V2_SPEC.md). Everything
not stated here keeps its version 2 meaning.

## Motivation

Every source span in a record carried its file's full path. On a
25,000-instruction disassembly built from one source file, the records named
that one path 145,043 times: 39% of a 58 MB document, and the largest single
cost for every consumer that parses it.

## Format

- The records section and the standalone `--instruction-records-output`
  document carry `"version":"3"` and a new `files` array: each distinct source
  path, as version 2 wrote it, once, in order of first use.
- A span's `file` is an integer index into `files`. This covers every span:
  `use`, `source.span`, `operand_source.span`, expression-node spans, term
  source spans and binding definitions.

```json
{"version":"3","files":["/work/game.asm","/work/include/ram.asm"],"records":[
  {"origin_id":1,"use":{"file":0,"line":12,"column":5,"end_line":12,"end_column":20}, "...": "..."}
]}
```

Version 2 is no longer produced and there is no compatibility switch.

## Performance

Like every change to assembler output, version 3 must not noticeably slow
assembly; see the Performance section of the README. Measured with
`tests/verify_fceux_projects.py` on the same 25,000-instruction disassembly:
median CPU seconds over five paired runs, with version 3 measured against each
baseline.

| Mode | Version 1 | Version 2 | Version 3 |
|---|---|---|---|
| Separate records output | 0.224 | 0.284 | 0.211–0.214 |
| Full xref with embedded records | 0.206 | 0.254 | 0.222–0.226 |
| Plain assembly | 0.073 | 0.072 | 0.070–0.074 |

The separate records document is 33.1 MB in version 1, 46.0 MB in version 2
and 35.1 MB in version 3.

Besides the file table, version 3 writes spans and their text without
per-span formatting or copies, and buffers the separate records output as the
xref already was. The facts version 2 added cost about 40 ms on this input.
With records embedded in the full xref, about 18 ms of that remains, within the
checker's 20 ms allowance.

## Verification

- Every test that inspects records resolves span files through the table and
  checks that its entries are distinct, all used, and listed in order of first
  use, including a program with two include files of the same name.
- Byte-identical output before and after the span-writing changes, on the
  disassembly above.
