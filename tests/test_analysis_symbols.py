#!/usr/bin/env python3
"""Analysis symbol and scope contracts, independent of debugger exports."""
import csv
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

XASM = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'xasm'
XLNK = Path(os.environ.get('XLNK', XASM.parent / 'xlnk')).resolve()


class AnalysisSymbols(unittest.TestCase):
    MEMBER_DECLARATIONS = ('.struc Point\npx .byte\npy .byte\n.ends\n'
                           '.struc Box\npoints .Point[4]\ntail .byte\n.ends\n'
                           'p .Point {1,2}\nbox .Box {}\nidx .Point {}\n')
    MEMBER_BYTES = bytes.fromhex('01 02') + bytes(11)

    def check_member_references(self, source, expected, data_refs, record_refs,
                                symbol_names, data_expressions=None, plain_symbols=None):
        # Text/CSV have no data-provenance option: folded type names and
        # volatile aliases are absent there even before this fix.
        if plain_symbols is None:
            plain_symbols = symbol_names
        for pure in (False, True):
            for debug in (False, True):
                for fmt in ('json', 'text', 'csv'):
                    with self.subTest(pure=pure, debug=debug, fmt=fmt), tempfile.TemporaryDirectory() as directory:
                        root = Path(directory)
                        (root / 'input.asm').write_text(source)
                        flags = ['--pure-binary'] if pure else []
                        if debug:
                            flags += ['--debug']
                        if fmt == 'json':
                            flags += ['--xref-data=true']
                        if pure and fmt == 'json':
                            flags += ['--xref-instructions=true', '--instruction-records-output=records',
                                      '--dependency-manifest=deps']
                        result = subprocess.run([str(XASM), 'input.asm', '-o', 'output',
                            '--xref=xref', f'--xref-format={fmt}', *flags],
                            cwd=root, capture_output=True, timeout=10)
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        self.assertEqual(result.stderr, b'')
                        if pure:
                            binary = (root / 'output').read_bytes()
                        else:
                            (root / 'link').write_text('link{file=output,origin=0}\n')
                            linked = subprocess.run([str(XLNK), 'link', '-o', 'linked'],
                                                    cwd=root, capture_output=True, timeout=10)
                            self.assertEqual(linked.returncode, 0, linked.stderr.decode())
                            binary = (root / 'linked').read_bytes()
                        self.assertEqual(binary, expected)
                        if fmt == 'json':
                            facts = json.loads((root / 'xref').read_text())
                            self.assertEqual({s['name'] for s in facts['symbols']}, symbol_names)
                            # Original data operands are collected even after their
                            # real expressions have folded to offsets or literals.
                            data = facts['data_directive_references']
                            self.assertEqual([r['referenced_symbols'] for r in data], data_refs)
                            if data_expressions is not None:
                                self.assertEqual([r['expression'] for r in data], data_expressions)
                            refs = [r for r in facts['references'] if r['opcode'] is None]
                            self.assertEqual({r['symbol'] for r in refs},
                                             {name for names in data_refs for name in names})
                            if pure:
                                records = facts['instruction_records']['records']
                                standalone = json.loads((root / 'records').read_text())['records']
                                self.assertEqual(standalone, records)
                                self.assertEqual([r['referenced_symbols'] for r in records], record_refs)
                                for record, names in zip(records, record_refs):
                                    offset = record['output_offset']
                                    self.assertEqual(bytes(record['bytes']),
                                                     binary[offset:offset + record['size']])
                                    for term in record['additive_terms']['terms']:
                                        if 'referenced_symbols' in term:
                                            self.assertTrue(set(term['referenced_symbols']) <= set(names))
                        elif fmt == 'csv':
                            with (root / 'xref.symbols.csv').open() as stream:
                                self.assertEqual({r['name'] for r in csv.DictReader(stream)}, plain_symbols)
                        else:
                            table = (root / 'xref').read_text().split('SYMBOLS\n')[1].split('REFERENCES\n')[0]
                            self.assertEqual({line.split(',')[0] for line in table.splitlines() if line}, plain_symbols)

    def test_member_names_are_not_global_references(self):
        cases = [
            ('p.px', 0, ['p']),
            ('p.py', 1, ['p']),
            ('box.points[1].py', 5, ['box']),
            ('box.points[(idx.py-idx)].px', 4, ['box', 'idx']),
            ('box.points[py].px', 2, ['box', 'py']),
            ('box.points[box.points[1].py-box].px', 8, ['box']),
            ('box.points[(py+0)].px', 2, ['box', 'py']),
            ('Point::py', 1, ['Point']),
            ('p.py+py', 1, ['p', 'py']),
        ]
        for expression, value, refs in cases:
            with self.subTest(expression=expression):
                self.check_member_references(
                    '.public py\npy:\n' + self.MEMBER_DECLARATIONS +
                    f'.word {expression}\nlda #<{expression}\n',
                    self.MEMBER_BYTES + value.to_bytes(2, 'little') + bytes((0xA9, value)),
                    [refs], [refs], {'p', 'box', 'idx', 'py'} | set(refs), [expression],
                    plain_symbols={'p', 'box', 'idx', 'py'})

    def test_member_names_do_not_add_references_to_same_named_labels(self):
        source = self.MEMBER_DECLARATIONS + 'px: .byte 9\n.word p.px\n.word px\nlda #<p.px\n'
        self.check_member_references(source, self.MEMBER_BYTES + bytes.fromhex('09 00 00 0D 00 A9 00'),
                                     [['p'], ['px']], [['p']], {'p', 'box', 'idx', 'px'},
                                     ['p.px', 'px'])
        self.source.write_text(source)
        facts = self.export(extra=('--xref-data=true',))
        px_refs = [r for r in facts['references'] if r['symbol'] == 'px']
        self.assertEqual(len(px_refs), 1)
        self.assertEqual(px_refs[0]['line'], source.splitlines().index('.word px') + 1)
        self.assertEqual(px_refs[0]['column'], 7)

    def test_enum_and_record_member_references_remain_scoped(self):
        enum_source = ('.enum Color\nRed = 1\nGreen\n.ende\n'
                       '.word Color::Red,Color::Green,Green\n'
                       'lda #Color::Red\nlda #Color::Green\nlda #Green\n')
        self.check_member_references(
            enum_source, bytes.fromhex('01 00 02 00 02 00 A9 01 A9 02 A9 02'),
            [['Color'], ['Color'], ['Green']], [['Color'], ['Color'], ['Green']],
            {'Color', 'Green'}, ['Color::Red', 'Color::Green', 'Green'], plain_symbols=set())
        self.source.write_text(enum_source)
        facts = self.export(extra=('--xref-data=true', '--xref-instructions=true'))
        bare_term = facts['instruction_records']['records'][-1]['additive_terms']['terms'][0]
        self.assertEqual(bare_term['binding']['kind'], 'enum_member')
        self.assertEqual(bare_term['binding']['enum'], 'Color')
        self.assertEqual(bare_term['name'], 'Green')
        self.check_member_references(
            '.record Flags hi:3,lo:5\n.word mask Flags::hi,mask Flags::lo\n'
            'lda #mask Flags::hi\nlda #mask Flags::lo\n',
            bytes.fromhex('E0 00 1F 00 A9 E0 A9 1F'),
            [['Flags'], ['Flags']], [['Flags'], ['Flags']], {'Flags'},
            ['MASK Flags::hi', 'MASK Flags::lo'], plain_symbols=set())

    def test_member_references_survive_expansion_and_scopes(self):
        body = '.word p.py\nlda #<box.points[1].py\n'
        for wrapper in ('{body}', '.macro Emit\n{body}.endm\nEmit\n',
                        '.rept 2\n{body}.endm\n', '.proc Routine\n{body}.endp\n'):
            with self.subTest(wrapper=wrapper):
                count = 2 if wrapper.startswith('.rept') else 1
                names = {'p', 'box', 'idx'}
                if wrapper.startswith('.proc'):
                    names.add('Routine')
                self.check_member_references(self.MEMBER_DECLARATIONS + wrapper.format(body=body),
                    self.MEMBER_BYTES + bytes.fromhex('01 00 A9 05') * count,
                    [['p']] * count, [['box']] * count, names)
        # Aliases remain the written references; their member expressions must
        # still evaluate correctly, before and after the definition.
        for declaration in ('Alias .equ p.py\n', '.define Alias p.py\n', 'Alias = p.py\n'):
            for forward in (False, True):
                if forward and declaration.startswith('Alias ='):
                    continue
                with self.subTest(declaration=declaration, forward=forward):
                    body = '.word Alias\nlda #<Alias\n'
                    source = self.MEMBER_DECLARATIONS + (body + declaration if forward else declaration + body)
                    self.check_member_references(source, self.MEMBER_BYTES + bytes.fromhex('01 00 A9 01'),
                        [['Alias']], [['Alias']], {'p', 'box', 'idx', 'Alias'},
                        plain_symbols={'p', 'box', 'idx'} if declaration.startswith('Alias =') else None)

    def test_tied_references_keep_every_use_in_source_order(self):
        repeated = '+'.join(['u'] * 10)
        bodies = (f'.word {repeated}\nlda #<({repeated})\n',
                  f'.rept 2\n.word {repeated}\n.endm\n',
                  f'.macro Emit\n.word {repeated}\n.endm\nEmit\nEmit\n')
        for body in bodies:
            for pure in (False, True):
                for debug in (False, True):
                    rows_by_format = {}
                    for fmt in ('json', 'text', 'csv'):
                        with self.subTest(body=body, pure=pure, debug=debug, fmt=fmt), tempfile.TemporaryDirectory() as d:
                            root = Path(d)
                            (root / 'input.asm').write_text('.public u\nu: nop\n' + body)
                            flags = (['--pure-binary'] if pure else []) + (['--debug'] if debug else [])
                            result = subprocess.run([str(XASM), 'input.asm', '-o', 'output',
                                                     '--xref=xref', f'--xref-format={fmt}', *flags],
                                                    cwd=root, capture_output=True, timeout=10)
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            self.assertEqual(result.stderr, b'')
                            if fmt == 'json':
                                refs = json.loads((root / 'xref').read_text())['references']
                                rows = [(r['symbol'], r['file'], r['line'], r['column']) for r in refs]
                            elif fmt == 'csv':
                                with (root / 'xref.refs.csv').open() as stream:
                                    rows = [(r['symbol'], r['file'], int(r['line']), int(r['column']))
                                            for r in csv.DictReader(stream)]
                            else:
                                lines = (root / 'xref').read_text().split('REFERENCES\n')[1].splitlines()
                                rows = []
                                for line in lines:
                                    symbol, loc, *_ = line.split(',')
                                    file, row, column = loc.rsplit(':', 2)
                                    rows.append((symbol, file, int(row), int(column)))
                            self.assertEqual(len(rows), 20)
                            self.assertTrue(all(r[0] == 'u' for r in rows))
                            # Each directive has ten uses with the same primary
                            # address/offset/symbol keys. Preserve duplicates.
                            for offset in (0, 10):
                                group = rows[offset:offset + 10]
                                self.assertEqual(group, sorted(group))
                                if '.macro' not in body:
                                    self.assertEqual(len({r[3] for r in group}), 10)
                            rows_by_format[fmt] = rows
                            if not pure:
                                (root / 'link').write_text('link{file=output,origin=0}\n')
                                result = subprocess.run([str(XLNK), 'link', '-o', 'linked'],
                                                        cwd=root, capture_output=True, timeout=10)
                                self.assertEqual(result.returncode, 0, result.stderr.decode())
                            expected = bytes.fromhex('EA 00 00 A9 00') if 'lda' in body else bytes.fromhex('EA 00 00 00 00')
                            self.assertEqual((root / ('output' if pure else 'linked')).read_bytes(), expected)
                    self.assertEqual(rows_by_format.get('json'), rows_by_format.get('text'))
                    self.assertEqual(rows_by_format.get('json'), rows_by_format.get('csv'))

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='xasm-symbols-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / 'input.asm'
        self.output = self.root / 'output.bin'
        self.xref = self.root / 'xref'

    def export(self, fmt='json', locals_on=False, anon_on=False, extra=()):
        for path in self.root.glob('xref*'):
            path.unlink()
        result = subprocess.run([str(XASM), '--pure-binary', str(self.source), '-o', str(self.output),
                                 f'--xref={self.xref}', f'--xref-format={fmt}',
                                 f'--xref-include-locals={str(locals_on).lower()}',
                                 f'--xref-include-anon={str(anon_on).lower()}', *extra], capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        if fmt == 'json':
            return json.loads(self.xref.read_bytes())
        if fmt == 'csv':
            with Path(str(self.xref) + '.symbols.csv').open() as stream:
                return {row['name'] for row in csv.DictReader(stream)}
        symbols = self.xref.read_text().split('SYMBOLS\n')[1].split('REFERENCES\n')[0]
        return {line.split(',')[0] for line in symbols.splitlines() if line}

    def test_scope_filters_preserve_visible_definitions_and_references(self):
        self.source.write_text('''.ORG $8000
@@Before:
    NOP
Main:
@@Loop:
    BNE @@Loop
-   NOP
    BNE -
    JSR Target
    RTS
Target:
    RTS
END
''')
        complete = self.export(locals_on=True, anon_on=True)
        self.assertEqual({row['scope'] for row in complete['symbols']}, {'global', 'local', 'anonymous'})
        binary = self.output.read_bytes()
        for locals_on in (False, True):
            for anon_on in (False, True):
                allowed = {'global'} | ({'local'} if locals_on else set()) | ({'anonymous'} if anon_on else set())
                symbols = [row for row in complete['symbols'] if row['scope'] in allowed]
                names = {row['name'] for row in symbols}
                refs = [row for row in complete['references'] if row['symbol'] in names]
                for fmt in ('json', 'text', 'csv'):
                    with self.subTest(fmt=fmt, locals=locals_on, anonymous=anon_on):
                        actual = self.export(fmt, locals_on, anon_on)
                        if fmt == 'json':
                            self.assertEqual(actual['symbols'], symbols)
                            self.assertEqual(actual['references'], refs)
                        else:
                            self.assertEqual(actual, names)
                        self.assertEqual(self.output.read_bytes(), binary)

    def test_forward_definitions_and_owners_survive_index_growth_and_sorting(self):
        count = 300
        blocks = [f'Label{i:03d}:\n@@Loop:\nNOP\nJMP Label{(i + 1) % count:03d}\n'
                  for i in reversed(range(count))]
        self.source.write_text('.ORG $8000\n' + ''.join(blocks) + 'END\n')
        data = self.export(locals_on=True, extra=('--xref-instructions=true', '--xref-include-owner=true'))
        globals_by_name = {row['name']: row for row in data['symbols'] if row['scope'] == 'global'}
        self.assertEqual(len(globals_by_name), count)
        self.assertEqual(len(data['symbols']), count * 2)
        expected_bytes = bytearray()
        for i in reversed(range(count)):
            name = f'Label{i:03d}'
            address = 0x8000 + (count - 1 - i) * 4
            self.assertTrue(globals_by_name[name]['defined'])
            self.assertEqual(globals_by_name[name]['definition']['cpu_address'], f'0x{address:04X}')
            target = 0x8000 + (count - 1 - (i + 1) % count) * 4
            expected_bytes.extend((0xEA, 0x4C, target & 255, target >> 8))
        self.assertEqual(self.output.read_bytes(), expected_bytes)
        records = data['instruction_records']['records']
        self.assertEqual(len(records), count * 2)
        for record in records:
            owner = f'Label{count - 1 - record["output_offset"] // 4:03d}'
            self.assertEqual(record['lexical_owner'], owner)
        for reference in data['references']:
            owner = f'Label{count - 1 - reference["use_output_offset"] // 4:03d}'
            self.assertEqual(reference['owner_routine'], owner)

    def test_pointer_address_view_preserves_sections_aliases_and_visibility(self):
        self.source.write_text('''.DATASEG
.ORG $10
APointer:
.DSB 2
.CODESEG
.ORG $10
ZPointer:
ZAlias:
.DSB 2
SameSegment:
STA $10
STA $11
LDA [$10],Y
.ORG $8000
Fallback:
STA $10
STA $11
LDA [$10],Y
.ORG $20
@@Hidden:
.DSB 2
STA $20
STA $21
LDA [$20],Y
END
''')
        for locals_on in (False, True):
            with self.subTest(locals=locals_on):
                data = self.export(locals_on=locals_on, extra=('--xref-data=true',))
                flows = data['indirect_data_flows']
                names = [flow['ptr_symbol'] for flow in flows]
                self.assertIn('ZAlias', names)  # Same segment wins over APointer.
                self.assertIn('APointer', names)  # Fallback keeps the first sorted alias.
                hidden = [row['name'] for row in data['symbols'] if row['scope'] == 'local']
                self.assertEqual(len(flows), 3 if locals_on else 2)
                if locals_on:
                    self.assertIn(hidden[0], names)

    def test_all_hidden_definitions_produce_empty_symbol_views(self):
        self.source.write_text('.ORG $8000\n@@Hidden:\n-\nRTS\nEND\n')
        data = self.export()
        self.assertEqual(data['symbols'], [])
        self.assertEqual(data['references'], [])
        for fmt in ('text', 'csv'):
            self.assertEqual(self.export(fmt), set())


if __name__ == '__main__':
    unittest.main()
