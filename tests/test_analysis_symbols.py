#!/usr/bin/env python3
"""Analysis symbol and scope contracts, independent of debugger exports."""
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

XASM = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'xasm'


class AnalysisSymbols(unittest.TestCase):
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

    def test_all_hidden_definitions_produce_empty_symbol_views(self):
        self.source.write_text('.ORG $8000\n@@Hidden:\n-\nRTS\nEND\n')
        data = self.export()
        self.assertEqual(data['symbols'], [])
        self.assertEqual(data['references'], [])
        for fmt in ('text', 'csv'):
            self.assertEqual(self.export(fmt), set())


if __name__ == '__main__':
    unittest.main()
