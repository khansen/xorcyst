#!/usr/bin/env python3
"""Check symbol-table balance and behavior without wall-clock thresholds."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]


class SymbolTable(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory(prefix='xasm-symtab-build-')
        cls.addClassCleanup(build.cleanup)
        root = Path(build.name)
        cls.executable = root / 'test-symtab'
        compiler = [*shlex.split(os.environ.get('CC', 'cc')), '-I', str(REPO), '-Wall',
                    *shlex.split(os.environ.get('TEST_SYMTAB_CFLAGS', '-O0 -g'))]
        # Keep real AST ownership/finalization and assembler dependencies. Only
        # rename the CLI entry point so the harness can supply its own main.
        main = root / 'xasm.o'
        command = [*compiler, '-Dmain=assembler_main', '-c', str(REPO / 'xasm.c'), '-o', str(main)]
        result = subprocess.run(command, capture_output=True)
        if result.returncode:
            raise AssertionError(result.stderr.decode())
        sources = re.search(r'^xasm_SOURCES = (.*?)(?=\n\n)',
                            (REPO / 'Makefile.am').read_text(), re.M | re.S)[1]
        sources = [REPO / ('tests/test_symtab_alloc.c' if name == 'symtab.c' else name)
                   for name in sources.replace('\\\n', ' ').split()
                   if name.endswith('.c') and name != 'xasm.c']
        command = [*compiler, *map(str, sources), str(REPO / 'tests/test_symtab.c'),
                   str(main), '-o', str(cls.executable)]
        if sys.platform == 'darwin':
            command += ['-framework', 'CoreFoundation']
        result = subprocess.run(command, capture_output=True)
        if result.returncode:
            raise AssertionError(result.stderr.decode())

    def run_case(self, case):
        result = subprocess.run([str(self.executable), case], capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr.decode())

    def test_single_and_double_rotations_and_small_root_deletions(self):
        self.run_case('rotations')

    def test_ordered_and_permuted_keys_stay_balanced_through_insert_and_delete(self):
        self.run_case('ordered')

    def test_deletion_preserves_surviving_entries_and_owned_payloads(self):
        self.run_case('ownership')

    def test_scope_lookup_and_type_enumeration(self):
        self.run_case('scopes')

    def test_growing_scope_stack_and_empty_stack_operations(self):
        self.run_case('stack')

    def test_allocation_failures_preserve_stack_entries_and_definition_ownership(self):
        self.run_case('allocation')


if __name__ == '__main__':
    unittest.main()
