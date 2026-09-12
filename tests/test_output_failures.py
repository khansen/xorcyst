#!/usr/bin/env python3
"""Run the real assembler with test-only I/O and analysis allocation faults."""
import collections
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
                   if word.endswith('.c') and word not in ('xasm.c', 'listing.c', 'fceux_nl.c')]
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

    def run_xasm(self, *extra, faults=None):
        environment = {key: value for key, value in os.environ.items() if not key.startswith('XASM_TEST_')}
        environment.update(faults or {})
        return subprocess.run([str(self.executable), '--pure-binary', str(self.source), '-o', str(self.output),
                               f'--fceux-nl-rom-prefix={self.root}/game.nes.',
                               f'--fceux-nl-ram-output={self.ram}', *extra],
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
        for operation in ('nl_fdopen', 'nl_ferror', 'nl_fclose', 'nl_rename'):
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
        previous = self.seed_outputs()
        result = self.run_xasm(*flags, faults={
            'XASM_TEST_ALLOC_PHASE': 'address', 'XASM_TEST_ALLOC_AT': '0'})
        self.assertGreater(result.returncode, 0, result.stderr.decode())
        self.assertIn(b'INJECT_ALLOC address 0 build_xref_address_index', result.stderr)
        self.assertIn(b'could not build xref data records', result.stderr)
        for path in (self.ram, self.bank0, self.bank1):
            self.assertEqual(path.read_bytes(), previous[path])
        self.assert_no_temporary_files()


if __name__ == '__main__':
    unittest.main()
