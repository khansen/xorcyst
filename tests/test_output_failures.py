#!/usr/bin/env python3
"""Run the real assembler with test-only I/O and analysis allocation faults."""
import collections
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


class OutputFailures(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory(prefix='xasm-fault-build-')
        cls.addClassCleanup(build.cleanup)
        cls.executable = Path(build.name) / 'xasm-faults'
        # Use the normal assembler source list, replacing only the translation
        # units that include the test wrappers. No production hooks are needed.
        sources = re.search(r'^xasm_SOURCES = (.*?)(?=\n\n)',
                            (REPO / 'Makefile.am').read_text(), re.M | re.S)[1]
        sources = [REPO / word for word in sources.replace('\\\n', ' ').split()
                   if word.endswith('.c') and word not in ('xasm.c', 'listing.c', 'symtab.c', 'fceux_nl.c')]
        sources += [REPO / 'tests/test_io_faults.c', REPO / 'tests/test_analysis_faults.c']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-I', str(REPO), '-Wall',
                   *shlex.split(os.environ.get('TEST_FAULT_CFLAGS', '-O0 -g')),
                   *map(str, sources), '-o', str(cls.executable)]
        if sys.platform == 'darwin':
            command += ['-framework', 'CoreFoundation']
        result = subprocess.run(command, capture_output=True)
        if result.returncode:
            raise AssertionError(result.stderr.decode())

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='xasm-fault-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / 'input.asm'
        self.output = self.root / 'output.bin'
        self.ram = self.root / 'game.nes.ram.nl'
        self.bank0 = self.root / 'game.nes.0.nl'
        self.bank1 = self.root / 'game.nes.1.nl'
        self.source.write_text('Port .EQU $10\n.ORG $8000\nFirst:\nSTA Port\n'
                               '.DSB $4000-($-$8000)\n.ORG $C000\nSecond:\nRTS\nEND\n')

    def run_xasm(self, *extra, faults=None, pure_binary=True, nl=True):
        environment = {key: value for key, value in os.environ.items() if not key.startswith('XASM_TEST_')}
        environment.update(faults or {})
        flags = ['--pure-binary'] if pure_binary else []
        if nl:
            flags += [f'--fceux-nl-rom-prefix={self.root}/game.nes.',
                      f'--fceux-nl-ram-output={self.ram}']
        return subprocess.run([str(self.executable), str(self.source), '-o', str(self.output), *flags, *extra],
                              capture_output=True, env=environment, timeout=15)

    def seed_outputs(self, extra=()):
        previous = {}
        for path in (self.output, self.ram, self.bank0, self.bank1, *extra):
            previous[path] = b'previous ' + path.name.encode() + b'\n'
            path.write_bytes(previous[path])
        return previous

    def assert_no_temporary_files(self):
        self.assertFalse(Path(str(self.output) + '.tmp').exists())
        for path in (self.ram, self.bank0, self.bank1):
            self.assertEqual(list(self.root.glob(path.name + '.*')), [])

    def test_binary_fdopen_failure_removes_created_stage_and_preserves_outputs(self):
        manifest = self.root / 'deps.json'
        for existing_stage in (False, True):
            with self.subTest(existing_stage=existing_stage):
                expected = self.seed_outputs([manifest])
                stage = Path(str(self.output) + '.tmp')
                if existing_stage:
                    stage.write_bytes(b'stale staging bytes')
                result = self.run_xasm(f'--dependency-manifest={manifest}',
                                       faults={'XASM_TEST_IO_FAILURE': 'binary_fdopen'})
                self.assertGreater(result.returncode, 0, result.stderr.decode())
                self.assertIn(b'INJECT_IO binary_fdopen', result.stderr)
                self.assertEqual({path: path.read_bytes() for path in expected}, expected)
                self.assert_no_temporary_files()

    def test_nl_io_failures_preserve_failed_and_later_destinations(self):
        ordered = (self.ram, self.bank0, self.bank1)
        result = self.run_xasm()
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        published = {path: path.read_bytes() for path in (self.output, *ordered)}
        self.assertEqual(published[self.ram], b'$0010#Port#\n')
        self.assertEqual(published[self.bank0], b'$8000#First#\n')
        self.assertEqual(published[self.bank1], b'$C000#Second#\n')
        manifest = self.root / 'deps.json'
        for operation in ('nl_mkstemp', 'nl_fdopen', 'nl_ferror', 'nl_fclose', 'nl_rename'):
            for fail_at in range(len(ordered)):
                with self.subTest(operation=operation, fail_at=fail_at):
                    previous = self.seed_outputs([manifest])
                    result = self.run_xasm(f'--dependency-manifest={manifest}', faults={
                        'XASM_TEST_IO_FAILURE': operation, 'XASM_TEST_IO_AT': str(fail_at)})
                    self.assertGreater(result.returncode, 0, result.stderr.decode())
                    self.assertIn(f'INJECT_IO {operation}'.encode(), result.stderr)
                    self.assertEqual(self.output.read_bytes(), published[self.output])
                    for index, path in enumerate(ordered):
                        self.assertEqual(path.read_bytes(), published[path] if index < fail_at else previous[path])
                    self.assertEqual(manifest.read_bytes(), previous[manifest])
                    self.assert_no_temporary_files()

    def test_binary_fstat_failure_cleans_only_created_stage(self):
        for existing_stage in (False, True):
            with self.subTest(existing_stage=existing_stage):
                previous = self.seed_outputs()
                stage = Path(str(self.output) + '.tmp')
                if existing_stage:
                    stage.write_bytes(b'previous staging bytes')
                result = self.run_xasm(faults={'XASM_TEST_IO_FAILURE': 'binary_fstat'})
                self.assertGreater(result.returncode, 0, result.stderr.decode())
                self.assertIn(b'INJECT_IO binary_fstat', result.stderr)
                self.assertEqual({path: path.read_bytes() for path in previous}, previous)
                if existing_stage:
                    self.assertEqual(stage.read_bytes(), b'previous staging bytes')
                    stage.unlink()
                self.assert_no_temporary_files()

    def test_constant_enumeration_allocation_failures(self):
        self.source.write_text('Alpha .EQU $10\nBeta .EQU $11\nGamma .EQU $12\n'
                               '.ORG $8000\nEntry:\nLDA Alpha\nSTA Beta\nLDA Gamma\nEND\n')
        xref = self.root / 'xref.json'
        manifest = self.root / 'deps.json'
        flags = (f'--xref={xref}', f'--dependency-manifest={manifest}')
        for nl in (False, True):
            baseline = self.run_xasm(*flags, nl=nl, faults={'XASM_TEST_ALLOC_PHASE': 'constants'})
            self.assertEqual(baseline.returncode, 0, baseline.stderr.decode())
            sites = re.findall(rb'ALLOC_SITE constants (\d+) symtab_list_type', baseline.stderr)
            self.assertEqual(len(sites), 4, baseline.stderr.decode())
            for index in sites:
                with self.subTest(nl=nl, index=index.decode()):
                    previous = self.seed_outputs([xref, manifest])
                    result = self.run_xasm(*flags, nl=nl, faults={
                        'XASM_TEST_ALLOC_PHASE': 'constants', 'XASM_TEST_ALLOC_AT': index.decode()})
                    self.assertIn(b'INJECT_ALLOC constants', result.stderr)
                    self.assertGreater(result.returncode, 0, result.stderr.decode())
                    self.assertEqual({path: path.read_bytes() for path in previous}, previous)
                    self.assert_no_temporary_files()

    def test_xref_analysis_failures_preserve_destination(self):
        self.source.write_text('.DATASEG\n.ORG $10\nPointer:\n.DSB 2\n.CODESEG\n.ORG $8000\n'
                               'Main:\nSTA Pointer\nSTA Pointer+1\nLDA [Pointer],Y\nEND\n')
        xref = self.root / 'xref.json'
        flags = (f'--xref={xref}', '--xref-data=true', '--xref-include-owner=true')
        for nl in (False, True):
            baseline = self.run_xasm(*flags, nl=nl, faults={'XASM_TEST_ALLOC_PHASE': 'xref'})
            self.assertEqual(baseline.returncode, 0, baseline.stderr.decode())
            sites = re.findall(rb'ALLOC_SITE xref (\d+) (\w+)', baseline.stderr)
            self.assertIn(b'build_xref_owner_index', [function for _, function in sites])
            self.assertIn(b'build_xref_address_index', [function for _, function in sites])
            for index, function in sites:
                with self.subTest(nl=nl, index=index.decode(), function=function.decode()):
                    previous = self.seed_outputs([xref])
                    result = self.run_xasm(*flags, nl=nl, faults={
                        'XASM_TEST_ALLOC_PHASE': 'xref', 'XASM_TEST_ALLOC_AT': index.decode()})
                    self.assertIn(b'INJECT_ALLOC xref', result.stderr)
                    self.assertGreater(result.returncode, 0, result.stderr.decode())
                    for path in (xref, self.ram, self.bank0, self.bank1):
                        self.assertEqual(path.read_bytes(), previous[path])
                    self.assert_no_temporary_files()
            print(f'Checked {len(sites)} xref analysis allocation failures with NL={nl}', flush=True)

    def test_xref_buffer_failure_preserves_content_and_close_failure_is_reported(self):
        xref = self.root / 'xref.json'
        flags = (f'--xref={xref}', '--xref-instructions=true')
        for count in (1, 256):
            with self.subTest(instructions=count):
                self.source.write_text('.ORG $8000\nEntry:\n' + 'NOP\n' * count + 'END\n')
                baseline = self.run_xasm(*flags, nl=False)
                self.assertEqual(baseline.returncode, 0, baseline.stderr.decode())
                self.assertEqual(xref.stat().st_size > 65536, count == 256)
                expected = json.loads(xref.read_bytes())
                del expected['build']['timestamp_utc']
                for operation in ('xref_setvbuf', 'xref_fclose'):
                    result = self.run_xasm(*flags, nl=False, faults={'XASM_TEST_IO_FAILURE': operation})
                    self.assertIn(f'INJECT_IO {operation}'.encode(), result.stderr)
                    if operation == 'xref_setvbuf':
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        actual = json.loads(xref.read_bytes())
                        del actual['build']['timestamp_utc']
                        self.assertEqual(actual, expected)
                    else:
                        self.assertGreater(result.returncode, 0, result.stderr.decode())
                        self.assertIn(b'could not emit complete xref records', result.stderr)

    def test_binary_completion_failures_preserve_outputs(self):
        manifest = self.root / 'deps.json'
        xref = self.root / 'xref.json'
        for pure_binary, nl in ((True, False), (True, True), (False, False)):
            self.source.write_text(('.ORG $8000\n' if pure_binary else '') + 'Entry:\nNOP\nRTS\nEND\n')
            flags = [f'--xref={xref}']
            if pure_binary:
                flags.append(f'--dependency-manifest={manifest}')
            for operation in ('binary_ferror', 'binary_fclose', 'binary_rename'):
                for existing_binary in (False, True):
                    with self.subTest(pure_binary=pure_binary, nl=nl, operation=operation,
                                      existing_binary=existing_binary):
                        previous = self.seed_outputs([manifest, xref])
                        if not existing_binary:
                            self.output.unlink()
                            del previous[self.output]
                        result = self.run_xasm(*flags, pure_binary=pure_binary, nl=nl,
                                               faults={'XASM_TEST_IO_FAILURE': operation})
                        self.assertGreater(result.returncode, 0, result.stderr.decode())
                        self.assertIn(f'INJECT_IO {operation}'.encode(), result.stderr)
                        self.assertEqual(self.output.exists(), existing_binary)
                        self.assertEqual({path: path.read_bytes() for path in previous}, previous)
                        self.assert_no_temporary_files()

    def test_successful_binary_replacement(self):
        for pure_binary, nl in ((True, False), (True, True), (False, False)):
            with self.subTest(pure_binary=pure_binary, nl=nl):
                self.source.write_text(('.ORG $8000\n' if pure_binary else '') + 'Entry:\nNOP\nRTS\nEND\n')
                result = self.run_xasm(pure_binary=pure_binary, nl=nl)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                expected = self.output.read_bytes()
                self.assertTrue(expected)
                previous = self.seed_outputs()
                result = self.run_xasm(pure_binary=pure_binary, nl=nl)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual(self.output.read_bytes(), expected)
                if not nl:
                    for path in (self.ram, self.bank0, self.bank1):
                        self.assertEqual(path.read_bytes(), previous[path])
                self.assert_no_temporary_files()

    def test_shared_collection_and_projection_allocation_failures(self):
        owner = 'Owner' + 'o' * 200
        local = '@@Buffer' + 'b' * 200
        self.source.write_text(f'.DATASEG\n.ORG $10\n{owner}:\n{local}:\n.DSB 2\n'
                               f'.CODESEG\n.ORG $8000\nSTA 1+{local}\n'
                               + ''.join(f'.ORG $8000\nLabel{i}:\n@@Loop:\nNOP\nJMP Label{(i+1)%35}\n'
                                         for i in range(35)) + 'END\n')
        csv = self.root / 'xref'
        flags = (f'--xref={csv}', '--xref-format=csv')
        destinations = [self.root / 'xref.symbols.csv', self.root / 'xref.refs.csv']
        for phase in ('collect', 'plan', 'prepare'):
            with self.subTest(phase=phase):
                baseline = self.run_xasm(*flags, faults={'XASM_TEST_ALLOC_PHASE': phase})
                self.assertEqual(baseline.returncode, 0, baseline.stderr.decode())
                sites = re.findall(rb'ALLOC_SITE \w+ (\d+) (\w+)', baseline.stderr)
                self.assertTrue(sites, baseline.stderr.decode())
                functions = collections.Counter(function.decode() for _, function in sites)
                if phase == 'collect':
                    for function in ('rebuild_xref_symbol_index', 'ensure_xref_symbol_capacity',
                                     'advance_xref_position', 'append_rendered_text'):
                        self.assertGreaterEqual(functions[function], 2, functions)
                    self.assertGreater(functions['str_concat'], 0, functions)
                for index, function in sites:
                    with self.subTest(index=index.decode(), function=function.decode()):
                        previous = self.seed_outputs(destinations)
                        result = self.run_xasm(*flags, faults={
                            'XASM_TEST_ALLOC_PHASE': phase, 'XASM_TEST_ALLOC_AT': index.decode()})
                        self.assertIn(b'INJECT_ALLOC ', result.stderr)
                        self.assertGreater(result.returncode, 0, result.stderr.decode())
                        self.assertEqual({path: path.read_bytes() for path in previous}, previous)
                        self.assert_no_temporary_files()
                print(f'Checked {len(sites)} shared-analysis allocation failures in {phase}', flush=True)

    def test_visible_address_index_allocation_failure_propagates(self):
        self.source.write_text('.DATASEG\n.ORG $10\nPointer:\n.DSB 2\n.CODESEG\n.ORG $8000\n'
                               'Main:\nSTA $10\nSTA $11\nLDA [$10],Y\nEND\n')
        xref = self.root / 'xref.json'
        flags = (f'--xref={xref}', '--xref-data=true')
        baseline = self.run_xasm(*flags, faults={'XASM_TEST_ALLOC_PHASE': 'address'})
        self.assertEqual(baseline.returncode, 0, baseline.stderr.decode())
        self.assertIn(b'ALLOC_SITE address 0 build_xref_address_index', baseline.stderr)
        previous = self.seed_outputs([xref])
        result = self.run_xasm(*flags, faults={
            'XASM_TEST_ALLOC_PHASE': 'address', 'XASM_TEST_ALLOC_AT': '0'})
        self.assertGreater(result.returncode, 0, result.stderr.decode())
        self.assertIn(b'INJECT_ALLOC address 0 build_xref_address_index', result.stderr)
        self.assertIn(b'could not build xref data records', result.stderr)
        for path in (xref, self.ram, self.bank0, self.bank1):
            self.assertEqual(path.read_bytes(), previous[path])
        self.assert_no_temporary_files()


if __name__ == '__main__':
    unittest.main()
