#!/usr/bin/env python3
"""Check frontend work bounds and output contracts without timing thresholds."""
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]


class FrontendPerformance(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory(prefix='xasm-frontend-build-')
        cls.addClassCleanup(build.cleanup)
        cls.executable = Path(build.name) / 'xasm'
        compiler = [*shlex.split(os.environ.get('CC', 'cc')), '-I', str(REPO), '-Wall', '-O0', '-g']
        objects = []
        for section in ('TEST_PARSER_WORK', 'TEST_SOURCE_CACHE_WORK', 'TEST_FRONTEND_MAIN'):
            obj = Path(build.name) / (section + '.o')
            command = [*compiler, '-D' + section, '-c', str(REPO / 'tests/test_frontend_work.c'), '-o', str(obj)]
            result = subprocess.run(command, capture_output=True)
            if result.returncode:
                raise AssertionError(result.stderr.decode())
            objects.append(obj)
        sources = re.search(r'^xasm_SOURCES = (.*?)(?=\n\n)',
                            (REPO / 'Makefile.am').read_text(), re.M | re.S)[1]
        sources = [REPO / name for name in sources.replace('\\\n', ' ').split()
                   if name.endswith('.c') and name not in ('parser.c', 'listing.c', 'xasm.c')]
        command = [*compiler, *map(str, sources + objects), '-o', str(cls.executable)]
        if sys.platform == 'darwin':
            command += ['-framework', 'CoreFoundation']
        result = subprocess.run(command, capture_output=True)
        if result.returncode:
            raise AssertionError(result.stderr.decode())

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='xasm-frontend-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / 'input.asm'
        self.output = self.root / 'output.prg'
        self.xref = self.root / 'xref.json'

    def run_measured(self, args):
        result = subprocess.run([str(self.executable), *map(str, args)], capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        match = re.search(rb'FRONTEND_WORK prefix_nodes=(\d+) source_rewinds=(\d+)', result.stderr)
        self.assertIsNotNone(match, result.stderr.decode())
        return result, tuple(map(int, match.groups()))

    def assemble(self, complete_xref=False):
        flags = []
        if complete_xref:
            flags = [f'--xref={self.xref}', '--xref-include-locals=true', '--xref-include-anon=true',
                     '--xref-data=true', '--xref-include-owner=true', '--xref-instructions=true']
        return self.run_measured(['--pure-binary', self.source, '-o', self.output, *flags])[1]

    def test_statement_appends_have_linear_work_with_and_without_xref(self):
        for count in (512, 2048):
            expected = bytes((i * 17) % 256 for i in range(count))
            self.source.write_text('.ORG $8000\n' + ''.join(f'.DB ${value:02X}\n; empty statement\n'
                                                          for value in expected) + 'END\n')
            for complete in (False, True):
                with self.subTest(statements=count, complete_xref=complete):
                    prefix_nodes, _ = self.assemble(complete)
                    self.assertEqual(self.output.read_bytes(), expected)
                    self.assertLessEqual(prefix_nodes, 4 * count,
                                         'parser traversed growing statement-list prefixes')

    def test_nested_and_empty_bodies_preserve_statement_chains_and_order(self):
        (self.root / 'nested.asm').write_text('.DB $0F\n')
        (self.root / 'included.asm').write_text('; leading empty statement\n.INCSRC "nested.asm"\n'
                                               'Included: .DB $11,$12\n\n')
        self.source.write_text('''; leading empty statement
.ORG $8000
.MACRO Emit Value
    .IF Value
        .DB Value
        .REPT 2
            .DB Value+1
        .ENDM
    .ELSE
        .DB $EE
    .ENDIF
.ENDM
.MACRO Empty
.ENDM
.IF 1
Before: .DB $10
    .INCSRC "included.asm"
    Emit $20
    .IF 0
        .DB $DD
    .ELIF 1
        .DB $24
    .ELSE
        .DB $CC
    .ENDIF
.ENDIF
Empty
.PROC Worker
    .DB $30
    NOP
.ENDP
Empty
After: .DB $40
Emit 0
END
''')
        for complete in (False, True):
            with self.subTest(complete_xref=complete):
                self.assemble(complete)
                self.assertEqual(self.output.read_bytes(), bytes.fromhex('10 0F 11 12 20 21 21 24 30 EA 40 EE'))
                if complete:
                    data = json.loads(self.xref.read_bytes())
                    definitions = {row['name']: row['definition']['cpu_address'] for row in data['symbols']}
                    self.assertEqual(definitions['Before'], '0x8000')
                    self.assertEqual(definitions['Included'], '0x8002')
                    self.assertEqual(definitions['Worker'], '0x8008')
                    self.assertEqual(definitions['After'], '0x800A')
                    records = data['instruction_records']['records']
                    self.assertEqual([(row['mnemonic'], row['output_offset']) for row in records], [('NOP', 9)])

    def test_cache_reuses_equal_lines_and_handles_backwards_reads_and_file_changes(self):
        first = self.root / 'first.asm'
        second = self.root / 'second.asm'
        first.write_bytes(b'first\n\n  third\r\nfourth\nfifth\n')
        second.write_text('other first\nother second\nother third\n')
        result, (_, rewinds) = self.run_measured(['--test-read-source-lines', first, second])
        self.assertEqual(result.stdout, b'  third\n  third\nfifth\nfirst\nother third\nfirst\n')
        self.assertEqual(rewinds, 1, 'only a backwards read within the current file needs a rewind')

    def test_repeated_data_operands_keep_source_spelling_without_rereads(self):
        self.source.write_text('Base .EQU $1234\n.ORG $8000\n' + '; padding\n' * 2048
                               + '.DW Base + $01, Base + $02, Base + $03\n.DW Base + $04\nEND\n')
        _, rewinds = self.assemble(complete_xref=True)
        self.assertEqual(rewinds, 0)
        self.assertEqual(self.output.read_bytes(), bytes.fromhex('35 12 36 12 37 12 38 12'))
        records = json.loads(self.xref.read_bytes())['data_directive_references']
        self.assertEqual([row['expression'] for row in records], [f'Base+${i:02X}' for i in range(1, 5)])
        self.assertEqual([row['emitted_value'] for row in records], list(range(0x1235, 0x1239)))
        self.assertEqual([row['target_displacement'] for row in records], [1, 2, 3, 4])


if __name__ == '__main__':
    unittest.main()
