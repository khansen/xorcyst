#!/usr/bin/env python3
"""Check AST and JSON work bounds and output contracts without timing thresholds."""
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


class BackendPerformance(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory(prefix='xasm-backend-build-')
        cls.addClassCleanup(build.cleanup)
        root = Path(build.name)
        cls.executable = root / 'xasm'
        compiler = [*shlex.split(os.environ.get('CC', 'cc')), '-I', str(REPO), '-Wall',
                    *shlex.split(os.environ.get('TEST_BACKEND_CFLAGS', '-O0 -g'))]
        source = (REPO / 'astnode.c').read_text()
        signature = 'int astnode_get_child_index(const astnode *p, const astnode *c)'
        if source.count(signature) != 1:
            raise AssertionError('expected one child-index definition to instrument')
        instrumented = root / 'astnode.c'
        instrumented.write_text(source.replace(signature, signature.replace(
            'astnode_get_child_index', 'test_real_child_index')))
        objects = []
        for section in ('TEST_AST_WORK', 'TEST_JSON_WORK', 'TEST_BACKEND_MAIN'):
            obj = root / (section + '.o')
            command = [*compiler, '-D' + section, f'-DTEST_AST_SOURCE="{instrumented}"',
                       '-c', str(REPO / 'tests/test_backend_work.c'), '-o', str(obj)]
            result = subprocess.run(command, capture_output=True)
            if result.returncode:
                raise AssertionError(result.stderr.decode())
            objects.append(obj)
        sources = re.search(r'^xasm_SOURCES = (.*?)(?=\n\n)',
                            (REPO / 'Makefile.am').read_text(), re.M | re.S)[1]
        sources = [REPO / name for name in sources.replace('\\\n', ' ').split()
                   if name.endswith('.c') and name not in ('astnode.c', 'listing.c', 'xasm.c')]
        command = [*compiler, *map(str, sources + objects), '-o', str(cls.executable)]
        if sys.platform == 'darwin':
            command += ['-framework', 'CoreFoundation']
        result = subprocess.run(command, capture_output=True)
        if result.returncode:
            raise AssertionError(result.stderr.decode())

    def run_measured(self, args, payload=None):
        result = subprocess.run([str(self.executable), *map(str, args)], input=payload,
                                capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        match = re.search(rb'BACKEND_WORK index_nodes=(\d+) json_calls=(\d+)', result.stderr)
        self.assertIsNotNone(match, result.stderr.decode())
        return result, tuple(map(int, match.groups()))

    def test_removal_preserves_tree_links_and_subtrees_with_linear_work(self):
        for count in (512, 2048):
            with self.subTest(nodes=count):
                _, (nodes, _) = self.run_measured(['--test-ast-removal', count])
                self.assertLessEqual(nodes, 4 * count, 'removal scanned growing child-list prefixes')

    def test_indexed_removal_preserves_positions_and_rejects_nonchildren(self):
        self.run_measured(['--test-indexed-removal'])

    def test_data_merging_has_linear_index_work_with_and_without_xref(self):
        with tempfile.TemporaryDirectory(prefix='xasm-data-merge-') as directory:
            root = Path(directory)
            source, output = root / 'input.asm', root / 'output.prg'
            for count in (512, 2048):
                data = bytes(i % 256 for i in range(count))
                source.write_text('.ORG $8000\n' + 'NOP\n' * count
                                  + ''.join(f'.DB ${value:02X}\n' for value in data) + 'END\n')
                for complete in (False, True):
                    flags = ([f'--xref={root}/xref.json', '--xref-include-locals=true',
                              '--xref-include-anon=true', '--xref-data=true',
                              '--xref-include-owner=true', '--xref-instructions=true'] if complete else [])
                    with self.subTest(statements=count, complete_xref=complete):
                        _, (nodes, _) = self.run_measured(['--pure-binary', source, '-o', output, *flags])
                        self.assertEqual(output.read_bytes(), b'\xea' * count + data)
                        self.assertLessEqual(nodes, 16 * count, 'data merging scanned retained statements')

    def test_json_escaping_preserves_all_bytes_and_embedded_nuls(self):
        for payload in (bytes(range(256)), b'\x00after nul\x00', b'"\\\b\f\n\r\t',
                        'ASCII Ω 🍄'.encode(), b'', b'nonterminated'):
            with self.subTest(payload=payload):
                result, _ = self.run_measured(['--test-json-string', len(payload)], payload)
                # Latin-1 maps each byte to one code point so even non-UTF-8 input
                # checks the serializer's existing byte-preservation contract.
                expected = json.dumps(payload.decode('latin1'), ensure_ascii=False).encode('latin1')
                # Preserve xasm's existing uppercase hex spelling in escapes.
                expected = re.sub(rb'\\u([0-9a-f]{4})', lambda m: b'\\u' + m[1].upper(), expected)
                self.assertEqual(result.stdout, expected)
        result, _ = self.run_measured(['--test-json-string', 'null'])
        self.assertEqual(result.stdout, b'""')

    def test_json_batches_plain_spans_between_escapes(self):
        for length in (1024, 65536):
            for parts in ((b'x' * length,), (b'\n', b'x' * length, b'\\', b'y' * length, b'"')):
                payload = b''.join(parts)
                with self.subTest(length=length, escaped=len(parts) > 1):
                    result, (_, calls) = self.run_measured(['--test-json-string', len(payload)], payload)
                    self.assertEqual(json.loads(result.stdout).encode(), payload)
                    self.assertLessEqual(calls, 8, 'JSON output issued a stdio call per ordinary byte')


if __name__ == '__main__':
    unittest.main()
