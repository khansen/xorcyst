#!/usr/bin/env python3
"""Pin v1 RAM permutation and label spans for D1; no v2 code is exercised."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import shlex
import subprocess

from freeze_baselines import digest, minimal_environment, save

SIZES = [1, 2, 1, 2, 1, 2, 4, 1, 2, 1, 1]
EXPECTED_ORDER = [0, 2, 4, 7, 9, 10, 3, 8, 1, 5, 6]
SPANS = [
    ('unlabelled-data', 'Tbl: .db 1,2,3\n.db 4\nOut: .byte sizeof(Tbl)\n', '01 02 03 04 04'),
    ('global-boundary', 'Tbl: .db 1,2,3\nNext: .db 4\nOut: .byte sizeof(Tbl)\n', '01 02 03 04 03'),
    ('local-boundary', 'Tbl: .db 1,2,3\n@@x: .db 4\nOut: .byte sizeof(Tbl)\n', '01 02 03 04 03'),
    ('anonymous-boundary', 'Tbl: .db 1,2,3\n+\n.db 4\nOut: .byte sizeof(Tbl)\n', '01 02 03 04 03'),
    ('procedure-boundary', '.public Next\nTbl: .db 1,2,3\n.proc Next\n.db 4\n.endp\nOut: .byte sizeof(Tbl)\n', '01 02 03 04 03'),
    ('self-value-width', 'Msg: .char "HELLO"\n.byte sizeof(Msg)\n', '48 45 4C 4C 4F 06'),
    ('storage-and-data', 'Buf: .dsb 2\n.db 9\nOut: .byte sizeof(Buf)\n', '00 00 09 03'),
    ('instruction-boundary', 'Tbl: .db 1,2,3\nnop\nOut: .byte sizeof(Tbl)\n', '01 02 03 EA 03'),
    ('adjacent-labels', 'First:\nSecond: .db 1,2,3\nOut: .byte sizeof(First),sizeof(Second)\n', '01 02 03 03 03'),
]


def permutation(indices):
    """Literal transcription of v1's last-pivot, >= partition semantics."""
    a = list(indices)
    def key(i):
        return (1 << 24) | ((0x10000 - SIZES[i]) & 0xffffffff)
    def sort(p, r):
        if p >= r:
            return
        pivot, i = key(a[r]), p - 1
        for j in range(p, r):
            if key(a[j]) >= pivot:
                i += 1
                a[i], a[j] = a[j], a[i]
        a[i + 1], a[r] = a[r], a[i + 1]
        q = i + 1
        sort(p, q - 1)
        sort(q + 1, r)
    sort(0, len(a) - 1)
    return a


def run(argv, cwd):
    result = subprocess.run([str(x) for x in argv], cwd=cwd,
                            env=minimal_environment(cwd, '/usr/bin:/bin'),
                            capture_output=True, timeout=30)
    row = {'argv': [str(x) for x in argv], 'returncode': result.returncode,
           'stdout': result.stdout.decode(errors='replace'),
           'stderr': result.stderr.decode(errors='replace')}
    if result.returncode:
        raise AssertionError(row)
    return row


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('tools', type=Path)
    p.add_argument('destination', type=Path)
    p.add_argument('--gate', type=Path)
    p.add_argument('--stable-linker', type=Path)
    args = p.parse_args()
    tools, root = args.tools.resolve(), args.destination.resolve()
    root.mkdir(parents=True, exist_ok=False)
    report = {'tools': {name: digest(tools / name) for name in ('xasm','xlnk')},
              'spans': {}, 'ram': {}, 'roms': {}}
    for name, source, expected in SPANS:
        work = root / name
        work.mkdir()
        (work / 'in.asm').write_text(source)
        rows = report['spans'][name] = {'source': source, 'expected': expected, 'runs': []}
        for mode in ('binary', 'linked'):
            argv = [tools / 'xasm', 'in.asm', '-o', 'out.bin' if mode == 'binary' else 'out.o']
            if mode == 'binary':
                argv.append('--pure-binary')
            record = {'mode': mode, 'assemble': run(argv, work)}
            if mode == 'linked':
                (work / 'link.s').write_text('link{file=out.o,origin=0}\n')
                (work / 'out.bin').unlink(missing_ok=True)
                record['link'] = run([tools / 'xlnk', 'link.s', '-o', 'out.bin'], work)
            record['bytes'] = (work / 'out.bin').read_bytes().hex(' ').upper()
            assert record['bytes'] == expected, (name, record)
            rows['runs'].append(record)
    # RAM size uses unlabeled storage; zero-size labels are preserved.
    for name, source, expected in (
        ('ram-span', '.dataseg\naa .byte\n.dsb 3\nbb .byte\n.codeseg\n.byte aa,bb\n', '11 10'),
        ('ram-zero', '.dataseg\nbuf:\naa .byte\nbb .byte\n.codeseg\n.byte buf,aa,bb\n', '10 10 11'),
    ):
        work = root / name; work.mkdir()
        (work / 'in.asm').write_text(source)
        assemble = run([tools / 'xasm', 'in.asm', '-o', 'in.o'], work)
        (work / 'link.s').write_text('ram{start=$10,end=$7FF}\nlink{file=in.o,origin=0}\n')
        link = run([tools / 'xlnk', 'link.s', '-o', 'out.bin'], work)
        data = (work / 'out.bin').read_bytes().hex(' ').upper()
        assert data == expected, (name, data)
        report['ram'][name] = {'source': source, 'expected': expected, 'bytes': data,
                               'assemble': assemble, 'link': link}
    assert permutation(range(len(SIZES))) == EXPECTED_ORDER
    for name, groups in (
        ('one-unit', [list(range(11))]),
        ('split-units', [list(range(5)), list(range(5,11))]),
        ('reversed-units', [list(range(5,11)), list(range(5))]),
    ):
        work = root / name; work.mkdir()
        source_order = sum(groups, [])
        order = permutation(source_order)
        addresses, pc = {}, 0x10
        for i in order:
            addresses[i] = pc; pc += SIZES[i]
        expected = b''.join(addresses[i].to_bytes(2, 'little') for i in source_order)
        sources, commands = {}, []
        for n, group in enumerate(groups):
            source = '.dataseg\n' + ''.join(f'ram{i} .byte[{SIZES[i]}]\n' for i in group)
            source += '.codeseg\n.word ' + ','.join(f'ram{i}' for i in group) + '\n'
            sources[str(n)] = source
            (work / f'{n}.asm').write_text(source)
            commands.append(run([tools / 'xasm', f'{n}.asm', '-o', f'{n}.o'], work))
        (work / 'link.s').write_text('ram{start=$10,end=$7FF}\n' + ''.join(
            f'link{{file={n}.o' + (',origin=0' if n == 0 else '') + '}\n' for n in range(len(groups))))
        commands.append(run([tools / 'xlnk', 'link.s', '-o', 'out.bin'], work))
        data = (work / 'out.bin').read_bytes()
        assert data == expected, (name, data.hex(), expected.hex())
        report['ram'][name] = {'sources': sources, 'sizes': SIZES, 'input_order': source_order,
                               'permutation': order, 'bytes': data.hex(' ').upper(), 'commands': commands}
        if args.stable_linker:
            run([args.stable_linker.resolve(), 'link.s', '-o', 'stable.bin'], work)
            stable = (work / 'stable.bin').read_bytes()
            assert stable != data
            report['ram'][name]['stable_bytes'] = stable.hex(' ').upper()
    if args.gate:
        gate = args.gate.resolve()
        metadata = json.loads((gate / 'gate.json').read_text())
        assert metadata['complete']
        report['gate_report'] = digest(gate / 'gate.json')
        if args.stable_linker:
            report['stable_linker'] = digest(args.stable_linker)
        for name, project in metadata['projects'].items():
            source = Path(project['runs'][0]['cwd'])
            build = root / 'roms' / name
            shutil.copytree(source, build)
            product = build / project['product']
            product.unlink(missing_ok=True)
            lines = (gate / 'logs' / f'{name}-1.log').read_text().splitlines()
            invocations = [shlex.split(line) for line in lines
                           if line.strip() and Path(line.split()[0]).name == 'xlnk']
            assert len(invocations) == 1, (name, invocations)
            link_args = invocations[0][1:]
            if '-o' in link_args:
                idx = link_args.index('-o')
                del link_args[idx:idx+2]
            link_args += ['-o', project['product']]
            # The final linker invocation uses the unchanged script and actual
            # product, overriding output so stale ROMs cannot satisfy assertions.
            reference = run([tools / 'xlnk', *link_args], build)
            actual = product.read_bytes()
            assert digest(product) == project['expected_rom'], name
            row = {'baseline': digest(product), 'link': reference}
            if args.stable_linker:
                product.unlink()
                row['stable_link'] = run([args.stable_linker.resolve(), *link_args], build)
                stable = product.read_bytes()
                row['stable'] = digest(product)
                row['different_bytes'] = sum(a != b for a,b in zip(actual, stable)) + abs(len(actual)-len(stable))
                assert bool(row['different_bytes']) == (name != 'bombsweeper'), (name, row)
            report['roms'][name] = row
            print(name, row.get('different_bytes', 0), 'stable-sort differing bytes', flush=True)
    save(root / 'observations.json', report)
    print('Verified', len(SPANS), 'span cases in both modes, 5 RAM cases and', len(report['roms']), 'ROMs.')


if __name__ == '__main__':
    main()
