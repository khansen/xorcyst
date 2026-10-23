#!/usr/bin/env python3
"""Pin assembler-time sizeof(label) behavior using v1, not a v2 implementation."""
import argparse
import json
from pathlib import Path
import re
import subprocess

from freeze_baselines import digest, minimal_environment, save

CASES = [
    {"id": "ram-adjacent", "source": ".dataseg\np1:\np2:\n.dsb 2\np3 .byte\n.codeseg\nlda #sizeof(p1)\n", "bytes": "A9 02", "v2": "A9 02"},
    {"id": "ram-interrupted", "source": ".dataseg\nbuf:\n.dsb 2\n.codeseg\nlda #sizeof(buf)\n.dataseg\n.dsb 3\nnxt .byte\n", "bytes": "A9 02", "v2": "A9 02"},
    {"id": "forward-label", "source": ".byte sizeof(Tbl)\nTbl: .db 1,2,3\n", "unknown": "Tbl", "line": 1, "v2": "size unknown"},
    {"id": "forward-count", "source": "Buf: .dsb N\n.byte sizeof(Buf)\nN .equ 3\n", "unknown": "Buf", "line": 2, "v2": "size unknown"},
    {"id": "imported-count", "source": ".extrn N : byte\nBuf: .dsb N\n.byte sizeof(Buf)\n", "unknown": "Buf", "line": 3, "v2": "size unknown"},
    {"id": "known-count", "source": "N .equ 3\nBuf: .dsb N\n.byte sizeof(Buf)\n", "bytes": "00 00 00 04", "v2": "00 00 00 04"},
    {"id": "known-string", "source": 'Msg: .char "HELLO"\n.byte sizeof(Msg)\n', "bytes": "48 45 4C 4C 4F 06", "v2": "48 45 4C 4C 4F 06"},
    {"id": "known-equ-string", "source": 'S .equ "HELLO"\nMsg: .char S\n.byte sizeof(Msg)\n', "bytes": "48 45 4C 4C 4F 06", "v2": "48 45 4C 4C 4F 06"},
    {"id": "known-define-string", "source": '.define S "HELLO"\nMsg: .char S\n.byte sizeof(Msg)\n', "bytes": "48 45 4C 4C 4F 06", "v2": "48 45 4C 4C 4F 06"},
    {"id": "wide-string", "source": 'Msg: .word "AB"\n.byte sizeof(Msg)\n', "bytes": "41 00 42 00 05", "v2": "41 00 42 00 05"},
    {"id": "forward-string", "source": 'Msg: .char S\n.byte sizeof(Msg)\nS .equ "HELLO"\n', "binary_error": True, "linked": "48 45 4C 4C 4F 02", "v2": "size unknown"},
    {"id": "forward-string-after", "source": 'Msg: .char S\nAfter: .byte sizeof(Msg),After\nS .equ "HELLO"\n', "binary_error": True, "linked": "48 45 4C 4C 4F 01 01", "v2": "size unknown"},
    {"id": "imported-string", "source": '.extrn S : byte\nMsg: .char S\n.byte sizeof(Msg)\n', "provider": '.public S\nS .equ "HELLO"\n', "binary_error": True, "linked": "48 45 4C 4C 4F 02", "v2": "size unknown"},
    {"id": "forward-number-kind", "source": 'Msg: .byte Value\n.byte sizeof(Msg)\nValue .equ 7\n', "bytes": "07 02", "v2": "07 02; local integer kind validated at unit end"},
    {"id": "imported-number-kind", "source": '.extrn Value : byte\nMsg: .byte Value\n.byte sizeof(Msg)\n', "provider": '.public Value\nValue .equ 7\n', "binary_error": True, "linked": "07 02", "v2": "size unknown: imported operand"},
    {"id": "string-defined-before-sizeof", "source": 'Msg: .char S\nS .equ "HELLO"\n.byte sizeof(Msg)\n', "binary_error": True, "linked": "48 45 4C 4C 4F 02", "v2": "48 45 4C 4C 4F 06; string length known at sizeof use"},
    {"id": "forward-pointer-table", "source": 'Tbl: .dw H1,H2\n.byte sizeof(Tbl)\nH1: rts\nH2: rts\n', "bytes": "05 00 06 00 05 60 60", "v2": "05 00 06 00 05 60 60; local address kinds validated at unit end"},
    {"id": "forward-pointer-lowbyte", "source": 'Tbl: .db <H,>H\n.byte sizeof(Tbl)\nH: rts\n', "bytes": "03 00 03 60", "v2": "03 00 03 60"},
    {"id": "imported-pointer-table", "source": '.extrn H : proc\nTbl: .dw H\n.byte sizeof(Tbl)\n', "provider": '.public H\n.proc H\nrts\n.endp\n', "binary_error": True, "linked": "03 00 03 60", "v2": "size unknown: explicit import, even with a proc annotation"},
    {"id": "implicit-import-pointer-table", "source": 'Tbl: .dw H\n.byte sizeof(Tbl)\n', "provider": '.public H\nH: rts\n', "binary_error": True, "linked": "03 00 03 60", "v2": "size unknown at sizeof use: name remains external at unit end"},
    {"id": "local-alias-of-import-before", "source": '.extrn Ext : byte\nK .equ Ext\nTbl: .db K\n.byte sizeof(Tbl)\n', "provider": '.public Ext\nExt .equ 7\n', "binary_error": True, "linked": "07 02", "v2": "size unknown: local alias still depends on an import"},
    {"id": "local-alias-of-import-after", "source": '.extrn Ext : byte\nTbl: .db K\n.byte sizeof(Tbl)\nK .equ Ext\n', "provider": '.public Ext\nExt .equ 7\n', "binary_error": True, "linked": "07 02", "v2": "size unknown: forward local alias still depends on an import"},
]

# These controls distinguish consumed/expanded statements from surviving scan
# boundaries. The expectation is about v1's stream at the sizeof use, not simply
# whether a directive emits bytes. Auxiliary inputs are written only in scratch.
for name, middle in [
        ('equ', 'K .equ 5'), ('assign', 'K = 5'), ('define', '.define K 5'),
        ('undef', '.define K 5\n.undef K'), ('extrn', '.extrn K : byte'),
        ('message', '.message "m"'), ('warning', '.warning "w"'),
        ('macro-definition', '.macro M\n.db 9\n.endm')]:
    CASES.append({'id': 'middle-' + name,
                  'source': 'Tbl: .db 1,2\n' + middle + '\n.db 3\n.byte sizeof(Tbl)\n',
                  'bytes': '01 02 03 04', 'v2': '01 02 03 04'})
for name, middle, expected in [
        ('if', '.if 1\n.db 3\n.endif', '01 02 03 04'),
        ('ifdef', '.define K 1\n.ifdef K\n.db 3\n.endif', '01 02 03 04'),
        ('if-false', '.if 0\n.db 9\n.endif\n.db 3', '01 02 03 04'),
        ('ifndef', '.ifndef Missing\n.db 4\n.endif\n.db 3', '01 02 04 03 05'),
        ('else', '.if 0\n.db 9\n.else\n.db 4\n.endif\n.db 3', '01 02 04 03 05'),
        ('elif', '.if 0\n.db 9\n.elif 1\n.db 4\n.else\n.db 8\n.endif\n.db 3', '01 02 04 03 05'),
        ('rept', '.rept 2\n.db 3\n.endm', '01 02 03 03 05'),
        ('while', 'I = 0\n.while I < 2\n.db 3\nI = I+1\n.endm', '01 02 03 03 05'),
        ('do-until', 'I = 0\n.do\n.db 4\nI = I+1\n.until I == 2\n.db 3', '01 02 04 04 03 06'),
        ('macro-call', '.macro M\n.db 9\n.endm\nM\n.db 3', '01 02 09 03 05')]:
    CASES.append({'id': 'middle-' + name,
                  'source': 'Tbl: .db 1,2\n' + middle + '\n.byte sizeof(Tbl)\n',
                  'bytes': expected, 'v2': expected})
for name, middle in [
        ('public', '.public Tbl'), ('struc', '.struc S\nf .byte\n.ends'),
        ('union', '.union U\nf .byte\n.ends'),
        ('record', '.record R hi:3,lo:5'),
        ('enum', '.enum E\nK\n.ende'), ('label', '.label Other')]:
    CASES.append({'id': 'middle-' + name,
                  'source': 'Tbl: .db 1,2\n' + middle + '\n.db 3\n.byte sizeof(Tbl)\n',
                  'bytes': '01 02 03 02', 'v2': '01 02 03 02'})
CASES.extend([
    {'id': 'middle-include-data', 'source': 'Tbl: .db 1,2\n.include "part.asm"\n.db 3\n.byte sizeof(Tbl)\n',
     'files': {'part.asm': '.db 9\n'}, 'bytes': '01 02 09 03 02', 'v2': '01 02 09 03 02'},
    {'id': 'middle-incbin', 'source': 'Tbl: .db 1,2\n.incbin "part.bin"\n.db 3\n.byte sizeof(Tbl)\n',
     'hex_files': {'part.bin': '07 08'}, 'bytes': '01 02 07 08 03 02', 'v2': '01 02 07 08 03 02'},
    {'id': 'middle-charmap', 'source': 'Tbl: .db 1,2\n.charmap "m.txt"\n.db 3\n.byte sizeof(Tbl)\n',
     'files': {'m.txt': 'A=1\n'}, 'bytes': '01 02 03 02', 'v2': '01 02 03 02'},
    {'id': 'middle-align-data', 'source': '.dataseg\nbuf: .dsb 2\n.align buf 2\n.dsb 1\n.codeseg\nlda #sizeof(buf)\n',
     'bytes': 'A9 02', 'v2': 'A9 02'},
])


def invoke(args, cwd):
    result = subprocess.run([str(a) for a in args], cwd=cwd,
                            env=minimal_environment(cwd, '/usr/bin:/bin'),
                            capture_output=True, timeout=20)
    assert result.returncode in (0, 1), result
    text = result.stderr.decode(errors='replace')
    assert 'syntax error' not in text, text
    return {'argv': [str(a) for a in args], 'status': result.returncode,
            'stdout': result.stdout.decode(errors='replace'), 'stderr': text}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('tools', type=Path)
    p.add_argument('destination', type=Path)
    args = p.parse_args()
    root, tools = args.destination.resolve(), args.tools.resolve()
    root.mkdir(parents=True, exist_ok=False)
    report = {'baseline': 'v1.9.12', 'script': digest(__file__),
              'tools': {n: digest(tools / n) for n in ('xasm','xlnk')}, 'cases': []}
    for case in CASES:
        row = {**case, 'runs': []}
        for binary in (False, True):
            for debug in (False, True):
                work = root / case['id'] / f'{"binary" if binary else "object"}-{int(debug)}'
                work.mkdir(parents=True)
                (work / 'input.asm').write_text(case['source'])
                for name, content in case.get('files', {}).items():
                    (work / name).write_text(content)
                for name, content in case.get('hex_files', {}).items():
                    (work / name).write_bytes(bytes.fromhex(content))
                output = work / ('output.bin' if binary else 'output.o')
                output.write_bytes(b'old output')
                argv = [tools / 'xasm', 'input.asm', '-o', output.name]
                if binary: argv.append('--pure-binary')
                if debug: argv.append('--debug')
                result = invoke(argv, work)
                record = {'binary': binary, 'debug': debug, 'assemble': result}
                failed = 'unknown' in case or (binary and case.get('binary_error', False))
                assert result['status'] == int(failed), (case['id'], record)
                if 'unknown' in case:
                    expected = f"input.asm:{case['line']}: error: size of `{case['unknown']}' is unknown"
                    assert expected in result['stderr'], record
                if failed:
                    assert output.read_bytes() == b'old output'
                    record['output_preserved'] = True
                else:
                    if not binary:
                        units = ['output.o']
                        if 'provider' in case:
                            (work / 'provider.asm').write_text(case['provider'])
                            provider = invoke([tools / 'xasm', 'provider.asm', '-o', 'provider.o'], work)
                            assert provider['status'] == 0, provider
                            record['provider'] = provider
                            units.append('provider.o')
                        (work / 'link.s').write_text('ram{start=$10,end=$7FF}\n' + ''.join(
                            f'link{{file={unit}' + (',origin=0' if i == 0 else '') + '}\n'
                            for i,unit in enumerate(units)))
                        output = work / 'linked.bin'
                        output.write_bytes(b'old output')
                        record['link'] = invoke([tools / 'xlnk', 'link.s', '-o', output.name], work)
                        assert record['link']['status'] == 0, record
                    record['bytes'] = output.read_bytes().hex(' ').upper()
                    assert record['bytes'] == case.get('bytes',case.get('linked')), record
                row['runs'].append(record)
        report['cases'].append(row)
    save(root / 'observations.json', report)
    print(f"Verified {len(CASES)} cases / {sum(len(c['runs']) for c in report['cases'])} mode/debug runs; located rejections preserve output.")


if __name__ == '__main__':
    main()
