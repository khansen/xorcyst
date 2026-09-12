#!/usr/bin/env python3
"""FCEUX export contracts using small, redistributable assembly fixtures."""
import json
import os
import shlex
import socket
import stat
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

XASM = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'xasm'


class FceuxNL(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='xasm-nl-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'input.asm'
        self.output = self.root / 'output.bin'
        self.prefix = self.root / 'game.nes.'
        self.ram = self.root / 'game.nes.ram.nl'

    def run_xasm(self, *options, pure=True):
        return subprocess.run([str(XASM), *(['--pure-binary'] if pure else []),
                               *options, str(self.source), '-o', str(self.output)], capture_output=True)

    def assemble(self, source, *, rom=True, ram=False, options=()):
        self.source.write_text(source)
        baseline = self.run_xasm(*options)
        self.assertEqual(baseline.returncode, 0, baseline.stderr.decode())
        expected = self.output.read_bytes()
        args = list(options)
        if rom:
            args.append(f'--fceux-nl-rom-prefix={self.prefix}')
        if ram:
            args.append(f'--fceux-nl-ram-output={self.ram}')
        result = self.run_xasm(*args)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(result.stdout, baseline.stdout)
        self.assertEqual(result.stderr, baseline.stderr)
        self.assertEqual(self.output.read_bytes(), expected)
        return result

    def bank(self, bank):
        return (self.root / f'game.nes.{bank:X}.nl').read_text()

    def test_local_only_page_and_unlabeled_page_keep_physical_numbers(self):
        self.assemble('''.ORG $8000
.DSB $4000
.ORG $8000
@@OnlyLocal:
    RTS
END
''')
        self.assertEqual(self.bank(0), '')
        self.assertEqual(self.bank(1), '$8000#@@OnlyLocal#\n')

    def test_crossing_page_needs_bytes_not_a_label_beyond_boundary(self):
        self.assemble('''.ORG $8000
Entry:
    .DSB $4000
@@Fixed:
    RTS
END
''')
        self.assertEqual(self.bank(0), '$8000#Entry#\n')
        self.assertEqual(self.bank(1), '$C000#Entry@@Fixed#\n')

    def test_end_labels_do_not_create_or_move_pages(self):
        self.assemble('''.ORG $8000
First:
    .DSB $4000
FirstEnd:
.ORG $8000
Second:
    .DSB $8000
SecondEnd:
END
''')
        self.assertEqual(self.bank(0), '$8000#First#\n')
        self.assertEqual(self.bank(1), '$8000#Second#\n')
        self.assertEqual(self.bank(2), '')
        self.assertFalse((self.root / 'game.nes.3.nl').exists())

    def test_multiple_orgs_can_share_one_physical_page(self):
        self.assemble('''.ORG $8000
First:
    RTS
.ORG $8001
Second:
    RTS
END
''')
        self.assertEqual(self.bank(0), '$8000#First#\n$8001#Second#\n')
        self.assertFalse((self.root / 'game.nes.1.nl').exists())

    def test_four_mapper_windows_use_physical_16k_pages(self):
        self.assemble(''.join(f'.ORG ${address:04X}\nWindow{i}:\n.DSB $2000\n'
                              for i, address in enumerate((0xA000, 0x8000, 0xE000, 0xC000))) + 'END\n')
        self.assertEqual(self.bank(0), '$8000#Window1#\n$A000#Window0#\n')
        self.assertEqual(self.bank(1), '$C000#Window3#\n$E000#Window2#\n')

    def test_bank_names_are_hexadecimal(self):
        self.assemble(''.join(f'.ORG $8000\nBank{i}:\n.DSB $4000\n' for i in range(17)) + 'END\n')
        self.assertEqual(self.bank(10), '$8000#Bank10#\n')
        self.assertEqual(self.bank(16), '$8000#Bank16#\n')
        self.assertFalse((self.root / 'game.nes.16.nl').exists())

    def test_mirror_names_globals_and_locals_in_either_window(self):
        for origin in ('$8000', '$C000'):
            with self.subTest(origin=origin):
                self.assemble(f'.ORG {origin}\nMain:\nNOP\n@@Loop:\nRTS\n.DSB $3FFE\nEND\n',
                              options=())
                result = self.run_xasm(f'--fceux-nl-rom-prefix={self.prefix}', '--fceux-nl-mirror-16k')
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual(self.bank(0), '$8000#Main#\n$8001#Main@@Loop#\n$C000#Main#\n$C001#Main@@Loop#\n')

    def test_mirror_checks_size_even_with_no_labels(self):
        for size in ('$4001', '$8000', '1'):
            with self.subTest(size=size):
                self.source.write_text(f'.ORG $8000\n.DSB {size}\nEND\n')
                old = self.root / 'game.nes.0.nl'
                old.write_text('previous\n')
                result = self.run_xasm(f'--fceux-nl-rom-prefix={self.prefix}', '--fceux-nl-mirror-16k')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b'exactly 16384', result.stderr)
                self.assertEqual(old.read_text(), 'previous\n')

    def test_ram_usage_and_aliases(self):
        self.assemble('''Buffer .EQU $10
Alias .EQU $10
Flag .EQU $11
Base .EQU $20
Field .EQU 2
CallTarget .EQU $12
.ORG $8000
    LDA #Flag
    LDA $13
    JSR CallTarget
    LDA Buffer
    STA Alias
    LDX Buffer, Y
    INC Base+Field, X
    LDA [Buffer],Y
END
''', rom=False, ram=True)
        self.assertEqual(self.ram.read_text(), '$0010#Alias#aka Buffer\n$0022#Base+Field#\n')

    def test_same_line_and_expanded_macro_operands(self):
        self.assemble('''One .EQU $10
Two .EQU $11
MACRO LOAD target
    LDA target
ENDM
.ORG $8000
    LDA One : STA Two
    LOAD One
    LOAD Two+1
END
''', rom=False, ram=True)
        self.assertEqual(self.ram.read_text(), '$0010#One#\n$0011#Two#\n$0012#Two+1#\n')

    def test_local_ram_labels_and_expressions_are_scope_independent(self):
        source = '''.DATASEG
.ORG $10
ProcA:
@@Buffer:
    .DSB 2
.CODESEG
.ORG $8000
    STA @@Buffer
    STA 1+@@Buffer
.DATASEG
ProcB:
@@Buffer:
    .DSB 2
.CODESEG
    STA @@Buffer
END
'''
        self.assemble(source, ram=True)
        expected = self.ram.read_text()
        self.assertEqual(expected, '$0010#ProcA#aka ProcA@@Buffer\n$0011#1+ProcA@@Buffer#\n$0012#ProcB#aka ProcB@@Buffer\n')
        for locals_on in ('false', 'true'):
            run = self.run_xasm(f'--fceux-nl-ram-output={self.ram}', f'--xref-include-locals={locals_on}')
            self.assertEqual(run.returncode, 0, run.stderr.decode())
            self.assertEqual(self.ram.read_text(), expected)

    def test_long_names_and_expressions_are_not_truncated(self):
        owner = 'Owner' + 'o' * 250
        local1 = '@@Local' + 'n' * 249 + 'A'
        local2 = '@@Local' + 'n' * 249 + 'B'
        self.assemble(f'.DATASEG\n.ORG $10\n{owner}:\n{local1}:\n.DSB 1\n{local2}:\n.DSB 1\n'
                      f'.CODESEG\n.ORG $8000\nSTA {local1}\nSTA {local2}\nEND\n', rom=False, ram=True)
        self.assertEqual(self.ram.read_text(), f'$0010#{owner}#aka {owner}{local1}\n$0011#{owner}{local2}#\n')

    def test_anonymous_labels_do_not_leak_nl_delimiters(self):
        self.assemble('.ORG $8000\nMain:\n-\nNOP\nBNE -\n@@Local:\nRTS\nEND\n',
                      options=('--xref-include-anon=true',))
        self.assertEqual(self.bank(0), '$8000#Main#\n$8003#Main@@Local#\n')

    def test_xref_outputs_and_instruction_records_are_unchanged(self):
        self.source.write_text('''Ram .EQU $10
.ORG $8000
Main:
    LDA Ram
@@Loop:
    BNE @@Loop
    LDA #<@@Loop
    STA $00
    LDA #>@@Loop
    STA $01
    LDA [$00],Y
    RTS
END
''')
        for fmt in ('json', 'text', 'csv'):
            for locals_on in ('true', 'false'):
                with self.subTest(fmt=fmt, locals=locals_on):
                    output = self.root / f'xref.{fmt}'
                    flags = [f'--xref={output}', f'--xref-format={fmt}', f'--xref-include-locals={locals_on}']
                    if fmt == 'json':
                        flags += ['--xref-data=true', '--xref-include-owner=true', '--xref-instructions=true']
                    plain = self.run_xasm(*flags)
                    self.assertEqual(plain.returncode, 0, plain.stderr.decode())
                    read_output = lambda: (output.read_text() if fmt != 'csv' else
                                           ''.join(p.read_text() for p in sorted(self.root.glob('xref.csv.*.csv'))))
                    before = read_output()
                    if fmt == 'csv':
                        for path in self.root.glob('xref.csv.*.csv'):
                            path.unlink()
                    else:
                        output.unlink()
                    run = self.run_xasm(*flags, f'--fceux-nl-rom-prefix={self.prefix}', f'--fceux-nl-ram-output={self.ram}')
                    self.assertEqual(run.returncode, 0, run.stderr.decode())
                    after = read_output()
                    if fmt == 'json':
                        before, after = json.loads(before), json.loads(after)
                        before['build'].pop('timestamp_utc')
                        after['build'].pop('timestamp_utc')
                    self.assertEqual(after, before)

    @staticmethod
    def normalize_json(value):
        if isinstance(value, dict):
            return {key: FceuxNL.normalize_json(child) for key, child in value.items()
                    if key != 'timestamp_utc'}
        if isinstance(value, list):
            return [FceuxNL.normalize_json(child) for child in value]
        return value

    def nl_files(self):
        return {path.name: path.read_bytes() for path in self.root.glob('game.nes.*.nl')}

    def test_nl_contents_do_not_depend_on_xref_configuration(self):
        self.source.write_text('''Port .EQU $2000
.DATASEG
.ORG $10
BufferOwner:
@@Buffer:
    .DSB 2
.CODESEG
.ORG $8000
    LDA @@Buffer
    STA Port
Entry:
@@Loop:
-   NOP
    BNE -
    BNE @@Loop
    .DSB $4000-($-$8000)
.ORG $C000
@@Fixed:
    RTS
END
''')
        nl = [f'--fceux-nl-rom-prefix={self.prefix}', f'--fceux-nl-ram-output={self.ram}']
        first = self.run_xasm(*nl)
        self.assertEqual(first.returncode, 0, first.stderr.decode())
        expected = self.nl_files()
        self.assertEqual(set(expected), {'game.nes.0.nl', 'game.nes.1.nl', 'game.nes.ram.nl'})
        for fmt in (None, 'json', 'text', 'csv'):
            for locals_on in ('false', 'true'):
                for anon_on in ('false', 'true'):
                    with self.subTest(fmt=fmt, locals=locals_on, anon=anon_on):
                        flags = [f'--xref-include-locals={locals_on}', f'--xref-include-anon={anon_on}']
                        if fmt is not None:
                            flags += [f'--xref={self.root / "xref"}', f'--xref-format={fmt}']
                        if fmt == 'json':
                            flags += ['--xref-data=true', '--xref-include-owner=true', '--xref-instructions=true']
                        for path in self.root.glob('game.nes.*.nl'):
                            path.unlink()
                        result = self.run_xasm(*nl, *flags)
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        self.assertEqual(self.nl_files(), expected)
        for path in self.root.glob('game.nes.*.nl'):
            path.unlink()
        result = self.run_xasm(*nl, f'--instruction-records-output={self.root / "instructions.json"}',
                               f'--dependency-manifest={self.root / "dependencies.json"}')
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(self.nl_files(), expected)

    def test_32k_nrom_without_mirroring(self):
        self.assemble('.ORG $8000\nLow:\n.DSB $4000\nHigh:\n.DSB $4000\nEND\n')
        self.assertEqual(self.bank(0), '$8000#Low#\n')
        self.assertEqual(self.bank(1), '$C000#High#\n')
        self.assertEqual(len(self.output.read_bytes()), 32768)

    def test_standalone_instruction_records_and_manifest_are_unchanged(self):
        (self.root / 'code.inc').write_text('Main:\nLDA Port\n@@Loop:\nBNE @@Loop\nRTS\n')
        self.source.write_text('Port .EQU $2000\n.ORG $8000\n.INCLUDE "code.inc"\nEND\n')
        records = self.root / 'instructions.json'
        manifest = self.root / 'dependencies.json'
        flags = [f'--instruction-records-output={records}', f'--dependency-manifest={manifest}']
        before = self.run_xasm(*flags)
        self.assertEqual(before.returncode, 0, before.stderr.decode())
        expected_records = records.read_bytes()
        expected_manifest = json.loads(manifest.read_bytes())
        self.assertEqual(expected_manifest['invocation'].pop('argv'), before.args)
        records.unlink()
        manifest.unlink()
        after = self.run_xasm(*flags, f'--fceux-nl-rom-prefix={self.prefix}',
                              f'--fceux-nl-ram-output={self.ram}')
        self.assertEqual(after.returncode, 0, after.stderr.decode())
        self.assertEqual(after.stdout, before.stdout)
        self.assertEqual(after.stderr, before.stderr)
        self.assertEqual(records.read_bytes(), expected_records)
        actual_manifest = json.loads(manifest.read_bytes())
        self.assertEqual(actual_manifest['invocation'].pop('argv'), after.args)
        self.assertEqual(actual_manifest, expected_manifest)

    def test_other_analysis_outputs_are_unchanged_with_nl(self):
        self.source.write_text('''Ram .EQU $10
.ORG $8000
Main:
    LDX #1
    LDA Table,X
    STA Ram
    JSR Sub
@@Loop:
    DEX
    BNE @@Loop
    RTS
Unused:
    NOP
Sub:
    RTS
Table:
    .DB 1,2,3
PointersLo:
    .DB <Sub
PointersHi:
    .DB >Sub
END
''')
        target = self.root / 'analysis'
        modes = [
            ('listing-text', [f'--listing={target}', '--listing-format=text']),
            ('listing-json', [f'--listing={target}', '--listing-format=json']),
            ('listing-ndjson', [f'--listing={target}', '--listing-format=ndjson']),
            ('summary-json', ['--xref-summary', f'--xref-summary-output={target}', '--xref-summary-format=json']),
            ('summary-text', ['--xref-summary', f'--xref-summary-output={target}', '--xref-summary-format=text']),
            ('summary-ndjson', ['--xref-summary', f'--xref-summary-output={target}', '--xref-summary-format=ndjson']),
            ('index-json', ['--analyze-index-patterns', f'--index-patterns-output={target}', '--index-patterns-format=json']),
            ('consumers-json', ['--data-consumers', f'--data-consumers-output={target}', '--data-consumers-format=json']),
            ('coverage-json', ['--analyze-data-coverage', f'--data-coverage-output={target}', '--data-coverage-format=json']),
        ]
        for mode, flags in modes:
            for locals_on in ('false', 'true'):
                with self.subTest(mode=mode, locals=locals_on):
                    options = [*flags, f'--xref-include-locals={locals_on}', f'--include-locals={locals_on}']
                    before = self.run_xasm(*options)
                    self.assertEqual(before.returncode, 0, before.stderr.decode())
                    expected_binary = self.output.read_bytes()
                    expected = target.read_text()
                    target.unlink()
                    self.output.unlink()
                    after = self.run_xasm(*options, f'--fceux-nl-rom-prefix={self.prefix}',
                                          f'--fceux-nl-ram-output={self.ram}')
                    self.assertEqual(after.returncode, 0, after.stderr.decode())
                    actual = target.read_text()
                    if mode.endswith('-json'):
                        expected, actual = map(lambda text: self.normalize_json(json.loads(text)), (expected, actual))
                    elif mode.endswith('-ndjson'):
                        expected, actual = [[self.normalize_json(json.loads(line)) for line in text.splitlines()]
                                            for text in (expected, actual)]
                    self.assertEqual(actual, expected)
                    self.assertEqual(after.stdout, before.stdout)
                    self.assertEqual(after.stderr, before.stderr)
                    self.assertEqual(self.output.read_bytes(), expected_binary)

    def test_labels_outside_cpu_range_and_dataseg_are_not_rom(self):
        self.assemble('''.DATASEG
.ORG $8000
HighRAM:
.DSB 1
.CODESEG
.ORG $8000
Code:
.DSB $8000
EndOfAddressSpace:
END
''')
        self.assertEqual(self.bank(0), '$8000#Code#\n')
        self.assertEqual(self.bank(1), '')

    def test_reject_non_prg_bytes_before_writing_nl(self):
        self.source.write_text('.ORG 0\n.DB 1,2,3,4\n.ORG $8000\nStart:\nRTS\nEND\n')
        result = self.run_xasm(f'--fceux-nl-rom-prefix={self.prefix}')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'raw PRG image', result.stderr)
        self.assertFalse((self.root / 'game.nes.0.nl').exists())

    def test_preflight_all_bank_paths_with_dependency_manifest(self):
        # The second bank would overwrite the assembly input. The first bank
        # must remain untouched when the later collision is discovered.
        self.source = self.root / 'game.nes.1.nl'
        self.source.write_text('.ORG $8000\n.DSB $8000\nEND\n')
        previous = self.root / 'game.nes.0.nl'
        previous.write_text('keep previous\n')
        manifest = self.root / 'deps.json'
        result = self.run_xasm(f'--fceux-nl-rom-prefix={self.prefix}', f'--dependency-manifest={manifest}')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'aliases', result.stderr)
        self.assertEqual(previous.read_text(), 'keep previous\n')
        self.assertTrue(self.source.read_text().startswith('.ORG'))
        self.assertFalse(manifest.exists())

    def test_rom_collisions_preserve_every_destination_before_publication(self):
        self.source.write_text('.ORG $8000\nMain:\n.DSB $8000\nEND\n')
        paths = {name: self.root / filename for name, filename in {
            'binary': 'output.bin', 'temporary': 'output.bin.tmp', 'listing': 'listing.json',
            'xref': 'xref.json', 'records': 'instructions.json', 'summary': 'summary.json',
            'index': 'index.json', 'consumers': 'consumers.json', 'coverage': 'coverage.json',
            'ram': 'game.nes.ram.nl', 'manifest': 'deps.json',
            'bank0': 'game.nes.0.nl', 'bank1': 'game.nes.1.nl',
        }.items()}
        for manifest in (False, True):
            roles = ['binary', 'listing', 'xref', 'summary', 'index', 'consumers', 'coverage', 'ram']
            if manifest:
                roles += ['records', 'manifest']
            for role in roles:
                with self.subTest(manifest=manifest, collision=role):
                    for name, path in paths.items():
                        path.write_text('previous ' + name + '\n')
                    expected = {path: path.read_bytes() for path in paths.values()}
                    destinations = dict(paths)
                    destinations[role] = paths['bank1']
                    self.output = destinations['binary']
                    flags = [f'--fceux-nl-rom-prefix={self.prefix}',
                             f'--fceux-nl-ram-output={destinations["ram"]}',
                             f'--listing={destinations["listing"]}', '--listing-format=json',
                             f'--xref={destinations["xref"]}',
                             '--xref-summary', f'--xref-summary-output={destinations["summary"]}',
                             '--analyze-index-patterns', f'--index-patterns-output={destinations["index"]}',
                             '--data-consumers', f'--data-consumers-output={destinations["consumers"]}',
                             '--analyze-data-coverage', f'--data-coverage-output={destinations["coverage"]}']
                    if manifest:
                        flags += [f'--dependency-manifest={destinations["manifest"]}',
                                  f'--instruction-records-output={destinations["records"]}']
                    result = self.run_xasm(*flags)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(b'aliases', result.stderr)
                    self.assertEqual({path: path.read_bytes() for path in expected}, expected)

    def test_csv_destinations_and_binary_temporary_are_validated_before_writes(self):
        self.source.write_text('.ORG $8000\nMain:\nRTS\nEND\n')
        csv = self.root / 'xref'
        symbol_file = self.root / 'xref.symbols.csv'
        reference_file = self.root / 'xref.refs.csv'
        bank = self.root / 'game.nes.0.nl'
        for collision in ('symbols', 'references', 'temporary'):
            with self.subTest(collision=collision):
                self.output = self.root / 'output.bin'
                paths = [self.output, symbol_file, reference_file, bank]
                for path in paths:
                    path.write_text('previous ' + path.name + '\n')
                temporary = Path(str(self.output) + '.tmp')
                if collision == 'temporary':
                    temporary.symlink_to(bank)
                    paths.append(temporary)
                else:
                    self.output = symbol_file if collision == 'symbols' else reference_file
                expected = {path: path.read_bytes() for path in paths}
                result = self.run_xasm(f'--xref={csv}', '--xref-format=csv',
                                       f'--fceux-nl-rom-prefix={self.prefix}')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b'aliases', result.stderr)
                self.assertEqual({path: path.read_bytes() for path in expected}, expected)

    def test_early_error_listings_validate_all_destinations(self):
        self.source.write_text('.ORG $8000\nMain:\nRTS\n.ERROR "stop"\nEND\n')
        bank = self.root / 'game.nes.0.nl'
        csv = self.root / 'xref'
        csv_symbols = self.root / 'xref.symbols.csv'
        csv_references = self.root / 'xref.refs.csv'
        for manifest in (False, True):
            paths = [self.output, bank, self.ram]
            if not manifest:
                paths += [csv_symbols, csv_references]
            for target in paths:
                with self.subTest(manifest=manifest, target=target.name):
                    for path in paths:
                        path.write_text('previous ' + path.name + '\n')
                    expected = {path: path.read_bytes() for path in paths}
                    flags = [f'--listing={target}', f'--fceux-nl-rom-prefix={self.prefix}',
                             f'--fceux-nl-ram-output={self.ram}']
                    if manifest:
                        flags += [f'--dependency-manifest={self.root}/deps.json']
                    else:
                        flags += [f'--xref={csv}', '--xref-format=csv']
                    result = self.run_xasm(*flags)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(b'aliases', result.stderr)
                    self.assertEqual({path: path.read_bytes() for path in paths}, expected)

    def test_safe_diagnostic_listings_survive_assembly_errors(self):
        listing = self.root / 'listing.txt'
        for source in ('.ORG $8000\nRTS\n.ERROR "stop"\nEND\n',
                       '.ORG $8000\nLDA Missing\nEND\n',
                       '.ORG $8000\nRTS\nLDA #\nEND\n'):
            self.source.write_text(source)
            for nl in (False, True):
                with self.subTest(source=source, nl=nl):
                    listing.write_text('previous listing\n')
                    self.output.write_bytes(b'previous binary')
                    flags = [f'--listing={listing}']
                    if nl:
                        flags += [f'--fceux-nl-rom-prefix={self.prefix}',
                                  f'--fceux-nl-ram-output={self.ram}']
                    result = self.run_xasm(*flags)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertNotIn(b'aliases', result.stderr)
                    self.assertIn('SOURCE CODE', listing.read_text())
                    self.assertEqual(self.output.read_bytes(), b'previous binary')
                    self.assertFalse(self.ram.exists())
                    self.assertFalse((self.root / 'game.nes.0.nl').exists())

    def test_unresolved_rom_size_cannot_publish_diagnostic_listing(self):
        self.source.write_text('.ORG $8000\n.DSB Missing\nEND\n')
        bank = self.root / 'game.nes.0.nl'
        bank.write_text('preserve symbols\n')
        self.output.write_bytes(b'preserve binary')
        result = self.run_xasm(f'--listing={bank}', f'--fceux-nl-rom-prefix={self.prefix}')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(bank.read_text(), 'preserve symbols\n')
        self.assertEqual(self.output.read_bytes(), b'preserve binary')

    def test_diagnostic_listing_metadata_preserves_requested_binary_path(self):
        self.source.write_text('.ORG $8000\nRTS\n.ERROR "stop"\nEND\n')
        listing = self.root / 'listing.json'
        for explicit_output in (False, True):
            baseline = None
            for protection in ('none', 'nl', 'manifest', 'both'):
                with self.subTest(output=explicit_output, protection=protection):
                    flags = [str(XASM), '--pure-binary', str(self.source),
                             f'--listing={listing}', '--listing-format=json']
                    if explicit_output:
                        flags += ['-o', str(self.output)]
                    if protection in ('nl', 'both'):
                        flags += [f'--fceux-nl-rom-prefix={self.prefix}', f'--fceux-nl-ram-output={self.ram}']
                    if protection in ('manifest', 'both'):
                        flags += [f'--dependency-manifest={self.root}/deps.json']
                    result = subprocess.run(flags, capture_output=True)
                    self.assertNotEqual(result.returncode, 0)
                    data = json.loads(listing.read_bytes())
                    self.assertEqual(data['output_file'], str(self.output) if explicit_output else '')
                    data.pop('timestamp_utc', None)
                    if baseline is None:
                        baseline = data
                    self.assertEqual(data, baseline)
                    self.assertFalse(self.output.exists())
                    self.assertFalse(self.source.with_suffix('.o').exists())

    def test_nonregular_binary_staging_nodes_fail_without_publishing(self):
        self.source.write_text('.ORG $8000\nRTS\nEND\n')
        stage = Path(str(self.output) + '.tmp')
        listing = self.root / 'listing.json'
        for kind in ('fifo', 'directory', 'socket'):
            with self.subTest(kind=kind):
                connection = None
                if kind == 'fifo':
                    os.mkfifo(stage)
                elif kind == 'directory':
                    stage.mkdir()
                else:
                    connection = socket.socket(socket.AF_UNIX)
                    previous_directory = os.getcwd()
                    try:
                        # AF_UNIX limits pathname length, including temp parents.
                        os.chdir(self.root)
                        connection.bind(stage.name)
                    except PermissionError:
                        connection.close()
                        self.skipTest('Unix socket creation is restricted in this environment')
                    finally:
                        os.chdir(previous_directory)
                try:
                    node_type = stat.S_IFMT(stage.stat().st_mode)
                    for protection in ('none', 'nl', 'manifest'):
                        with self.subTest(protection=protection):
                            paths = [self.output, listing, self.ram]
                            for path in paths:
                                path.write_bytes(b'preserve ' + path.name.encode())
                            expected = {path: path.read_bytes() for path in paths}
                            flags = [f'--listing={listing}', '--listing-format=json']
                            if protection == 'nl':
                                flags += [f'--fceux-nl-ram-output={self.ram}']
                            if protection == 'manifest':
                                flags += [f'--dependency-manifest={self.root}/deps.json']
                            result = subprocess.run([str(XASM), '--pure-binary', str(self.source),
                                                     '-o', str(self.output), *flags], capture_output=True, timeout=5)
                            self.assertNotEqual(result.returncode, 0)
                            self.assertIn(b'not a regular file', result.stderr)
                            self.assertEqual({path: path.read_bytes() for path in paths}, expected)
                            self.assertEqual(stat.S_IFMT(stage.stat().st_mode), node_type)
                finally:
                    if connection is not None:
                        connection.close()
                    if kind == 'directory':
                        stage.rmdir()
                    else:
                        stage.unlink()

    def test_diagnostic_listing_still_reserves_implicit_binary_destination(self):
        self.source.write_text('.ORG $8000\nRTS\n.ERROR "stop"\nEND\n')
        implicit_output = self.source.with_suffix('.o')
        for manifest in (False, True):
            with self.subTest(manifest=manifest):
                implicit_output.write_bytes(b'preserve binary')
                flags = [str(XASM), '--pure-binary', str(self.source),
                         f'--listing={implicit_output}', f'--fceux-nl-ram-output={self.ram}']
                if manifest:
                    flags += [f'--dependency-manifest={self.root}/deps.json']
                result = subprocess.run(flags, capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b'aliases', result.stderr)
                self.assertEqual(implicit_output.read_bytes(), b'preserve binary')

    def test_future_filesystem_equivalent_destinations_are_rejected(self):
        self.source.write_text('Port .EQU $2000\n.ORG $8000\nSTA Port\nEND\n')
        for name, alias in (('out.bin', 'OUT.BIN'), ('é.bin', 'e\u0301.bin'),
                            ('å.bin', 'Å.bin'), ('ß.bin', 'ss.bin'), ('ς.bin', 'σ.bin')):
            with self.subTest(name=name, alias=alias):
                probe = self.root / name
                probe.write_bytes(b'probe')
                equivalent = (self.root / alias).exists()
                probe.unlink()
                if not equivalent:
                    continue
                self.output = probe
                result = self.run_xasm(f'--fceux-nl-ram-output={self.root / alias}')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b'aliases', result.stderr)
                self.assertFalse(self.output.exists())
                self.assertFalse((self.root / alias).exists())

    def test_binary_staging_symlinks_are_rejected_before_publication(self):
        self.source.write_text('.ORG $8000\nRTS\nEND\n')
        stage = Path(str(self.output) + '.tmp')
        listing = self.root / 'listing.txt'
        for target in (self.ram, self.root / 'unrelated', self.root / 'existing'):
            with self.subTest(target=target.name):
                if target.name == 'existing':
                    target.write_bytes(b'preserve target')
                stage.unlink(missing_ok=True)
                stage.symlink_to(target)
                listing.write_bytes(b'preserve listing')
                result = self.run_xasm(f'--listing={listing}', f'--fceux-nl-ram-output={self.ram}')
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(self.output.exists())
                self.assertTrue(stage.is_symlink())
                self.assertEqual(listing.read_bytes(), b'preserve listing')
                if target.name == 'existing':
                    self.assertEqual(target.read_bytes(), b'preserve target')
                else:
                    self.assertFalse(target.exists())

    def test_error_listing_cannot_overwrite_comparison_reference(self):
        self.source.write_text('.ORG $8000\nRTS\n.ERROR "stop"\nEND\n')
        reference = self.root / 'reference.bin'
        for manifest in (False, True):
            with self.subTest(manifest=manifest):
                reference.write_bytes(b'preserve reference')
                flags = [f'--compare={reference}', f'--listing={reference}',
                         f'--fceux-nl-ram-output={self.ram}']
                if manifest:
                    flags += [f'--dependency-manifest={self.root}/deps.json']
                result = self.run_xasm(*flags)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b'aliases', result.stderr)
                self.assertEqual(reference.read_bytes(), b'preserve reference')

    def test_dangling_listing_symlinks_cannot_alias_future_nl_files(self):
        self.source.write_text('.ORG $8000\nMain:\nRTS\nEND\n')
        listing = self.root / 'listing.txt'
        for filename in ('game.nes.0.nl', 'game.nes.ram.nl'):
            for manifest in (False, True):
                with self.subTest(filename=filename, manifest=manifest):
                    listing.unlink(missing_ok=True)
                    listing.symlink_to(filename)
                    self.output.write_bytes(b'previous binary')
                    flags = [f'--listing={listing}', f'--fceux-nl-rom-prefix={self.prefix}',
                             f'--fceux-nl-ram-output={self.ram}']
                    if manifest:
                        flags += [f'--dependency-manifest={self.root}/deps.json']
                    result = self.run_xasm(*flags)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(b'aliases', result.stderr)
                    self.assertEqual(self.output.read_bytes(), b'previous binary')
                    self.assertTrue(listing.is_symlink())
                    self.assertFalse(listing.exists())

    def test_anonymous_memory_operands_are_excluded_inside_and_outside_macros(self):
        bodies = ('-\n.DB 0\nLDA -\n', 'LDA +\n+\n.DB 0\n',
                  '-\n.DB 0\nLDA (-)+Port\n', 'LDA Port+(+)\n+\n.DB 0\n')
        for body in bodies:
            for macro in (False, True):
                source = ('MACRO TEST\n' + body + 'ENDM\n.ORG $10\nTEST\n') if macro else '.ORG $10\n' + body
                source = 'Port .EQU $2000\n' + source + 'LDA Port\nEND\n'
                for anonymous in ('false', 'true'):
                    with self.subTest(body=body, macro=macro, anonymous=anonymous):
                        self.assemble(source, rom=False, ram=True,
                                      options=(f'--xref-include-anon={anonymous}',))
                        self.assertEqual(self.ram.read_text(), '$2000#Port#\n')

    def test_output_aliases_are_rejected_without_a_manifest(self):
        self.source.write_text('.ORG $8000\nMain:\nRTS\nEND\n')
        original = self.source.read_bytes()
        aliases = (self.source, self.output)
        for target in aliases:
            with self.subTest(target=target.name):
                result = self.run_xasm(f'--fceux-nl-ram-output={target}')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b'aliases', result.stderr)
                self.assertEqual(self.source.read_bytes(), original)
        alias = self.root / 'game.nes.0.nl'
        alias.symlink_to(self.source)
        result = self.run_xasm(f'--fceux-nl-rom-prefix={self.prefix}')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'aliases', result.stderr)
        self.assertEqual(self.source.read_bytes(), original)

    def test_colliding_future_outputs_and_csv_files_are_rejected(self):
        self.source.write_text('.ORG $8000\nMain:\nRTS\nEND\n')
        result = self.run_xasm(f'--fceux-nl-rom-prefix={self.prefix}',
                              f'--fceux-nl-ram-output={self.root}/./game.nes.0.nl')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'aliases', result.stderr)
        self.assertFalse((self.root / 'game.nes.0.nl').exists())
        csv = self.root / 'xref'
        result = self.run_xasm(f'--xref={csv}', '--xref-format=csv',
                              f'--fceux-nl-ram-output={csv}.symbols.csv')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'aliases', result.stderr)
        self.assertFalse((self.root / 'xref.symbols.csv').exists())

    def test_output_extents_include_data_storage_and_binary_includes(self):
        (self.root / 'blob.bin').write_bytes(bytes(0x3FFC))
        self.assemble('.ORG $8000\n.INCBIN "blob.bin"\n.DB 1\n.DW 2\n.DSB 1\n'
                      '.ORG $8000\nSecond:\nRTS\nEND\n')
        self.assertEqual(self.bank(0), '')
        self.assertEqual(self.bank(1), '$8000#Second#\n')

    def test_long_compound_expression_uses_the_ast(self):
        term = 'Zero' + 'z' * 250
        expression = 'Base' + ('+' + term) * 5
        self.assemble(f'Base .EQU $10\n{term} .EQU 0\n.ORG $8000\nLDA {expression}\nEND\n',
                      rom=False, ram=True)
        self.assertGreater(len(self.ram.read_text()), 1024)
        self.assertEqual(self.ram.read_text().count(term), 5)
        self.assertTrue(self.ram.read_text().startswith('$0010#'))

    def test_sizeof_expression_and_rmw_only_reference(self):
        self.assemble('Buffer .EQU $10\n.ORG $8000\nINC Buffer+sizeof("foo")\nEND\n',
                      rom=False, ram=True)
        self.assertEqual(self.ram.read_text(), '$0013#Buffer+SIZEOF("foo")#\n')

    def test_macro_local_labels_and_operand_scopes(self):
        self.assemble('''MACRO LOOP
@@Again:
    NOP
    BNE @@Again
ENDM
.ORG $8000
Main:
    LOOP
    LOOP
END
''')
        self.assertEqual(self.bank(0), '$8000#Main#aka Main@@Again\n$8003#Main@@Again#\n')

    def test_export_does_not_delete_unowned_bank_files(self):
        previous = self.root / 'game.nes.1.nl'
        previous.write_text('hand-written symbols\n')
        self.assemble('.ORG $8000\nMain:\nRTS\nEND\n')
        self.assertEqual(previous.read_text(), 'hand-written symbols\n')

    def test_allocation_failures_are_errors(self):
        executable = self.root / 'test-nl-alloc'
        source = Path(__file__).with_name('test_fceux_nl_alloc.c')
        compile_result = subprocess.run([*shlex.split(os.environ.get('CC', 'cc')),
                                         '-std=c99', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra',
                                         str(source), '-o', str(executable)], capture_output=True)
        self.assertEqual(compile_result.returncode, 0, compile_result.stderr.decode())
        result = subprocess.run([str(executable), str(self.prefix), str(self.ram)], capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b'allocation failure sites', result.stdout)

    def test_invalid_cli_and_failed_output(self):
        self.source.write_text('.ORG $8000\nMain:\nRTS\nEND\n')
        for flags, pure in ((['--fceux-nl-mirror-16k'], True),
                            (['--fceux-nl-rom-prefix='], True),
                            (['--fceux-nl-ram-output='], True),
                            ([f'--fceux-nl-rom-prefix={self.prefix}'], False)):
            with self.subTest(flags=flags):
                result = self.run_xasm(*flags, pure=pure)
                self.assertEqual(result.returncode, 2, result.stderr.decode())
        result = self.run_xasm(f'--fceux-nl-ram-output={self.root / "missing" / "ram.nl"}')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'could not write', result.stderr)


if __name__ == '__main__':
    unittest.main()
