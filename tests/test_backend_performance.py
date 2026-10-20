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
        source = source.replace(signature, signature.replace(
            'astnode_get_child_index', 'test_real_child_index'))
        source = source.replace('        n->parent = p;',
                                '        test_list_nodes++; n->parent = p;')
        source = source.replace('s = s->next_sibling) ;',
                                's = s->next_sibling) test_list_nodes++;')
        instrumented.write_text(source)
        processor = (REPO / 'astproc.c').read_text()
        signature = 'static astnode *substitute_ident(astnode *expr)\n{'
        if processor.count(signature) != 1:
            raise AssertionError('expected one constant-substitution definition')
        processor = processor.replace(signature, signature +
            '\n    extern unsigned long test_constant_expansions;\n    test_constant_expansions++;')
        processor = processor.replace('entries = calloc(capacity, sizeof(*entries));',
            'entries = getenv("XASM_TEST_CONSTANT_CACHE_FAIL") ? NULL : calloc(capacity, sizeof(*entries));')
        measured_processor = root / 'astproc.c'
        measured_processor.write_text(processor)
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
                   if name.endswith('.c') and name not in ('astnode.c', 'astproc.c', 'listing.c', 'xasm.c')]
        sources.append(measured_processor)
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

    def test_ast_splicing_preserves_ownership_and_order(self):
        self.run_measured(['--test-ast-splicing'])

    def test_xref_reference_sort_has_a_total_order_for_tied_uses(self):
        self.run_measured(['--test-xref-reference-order'])

    def test_aggregate_list_work_scales_linearly(self):
        with tempfile.TemporaryDirectory(prefix='xasm-aggregate-work-') as directory:
            root = Path(directory)
            source, output = root / 'input.asm', root / 'output.bin'
            for elements in (1, 2):
                for kind in ('literal', 'named', 'forward'):
                    for debug in (False, True):
                        samples = []
                        for count in (256, 1024):
                            declaration = str(elements) if kind == 'literal' else 'Count'
                            definition = f'Count={elements}\n'
                            body = ('.struc Point\npx .byte\npy .byte\n.ends\n'
                                    f'.struc Array\nitems .Point[{declaration}]\ntail .byte\n.ends\n'
                                    + '.Array {{1,2},$5A}\n' * count + 'After: .word After\n')
                            source.write_text((definition if kind == 'named' else '') + body
                                              + (definition if kind == 'forward' else ''))
                            with self.subTest(elements=elements, kind=kind, debug=debug, count=count):
                                flags = ['--debug'] if debug else []
                                result, (indices, _) = self.run_measured([
                                    '--pure-binary', source, '-o', output, *flags])
                                match = re.search(rb'LIST_WORK nodes=(\d+)', result.stderr)
                                self.assertIsNotNone(match, result.stderr.decode())
                                work = indices + int(match[1])
                                samples.append(work)
                                element = b'\x01\x02' + b'\0\0' * (elements - 1) + b'\x5A'
                                expected = element * count
                                expected += len(expected).to_bytes(2, 'little')
                                self.assertEqual(output.read_bytes(), expected)
                                self.assertLessEqual(work, 400 * count)
                        self.assertEqual(len(samples), 2)
                        self.assertLessEqual(samples[1], samples[0] * 4.1,
                                             'list work grew faster than the instance count')

    def test_repeated_constant_aliases_have_bounded_expansion_work(self):
        with tempfile.TemporaryDirectory(prefix='xasm-constant-work-') as directory:
            root = Path(directory)
            source, output = root / 'input.asm', root / 'output.bin'
            for depth in (12, 20):
                aliases = ''.join(f'A{i} .equ A{i+1}+A{i+1}\n' for i in range(depth))
                aliases += f'A{depth}=1\n'
                for field in (False, True):
                    if field:
                        body = ('.struc Point\npx .byte\npy .byte\n.ends\n'
                                '.struc Array\nitems .Point[Count]\ntail .byte\n.ends\n'
                                '.Array {{1,2},$5A}\n')
                        expected = bytes.fromhex('01 02 00 00 5A')
                    else:
                        body, expected = '.byte Count+Count\n', b'\x04'
                    source.write_text(body + f'Count .equ (A0 >> {depth})+1\n' + aliases)
                    with self.subTest(depth=depth, field=field):
                        result, _ = self.run_measured(['--pure-binary', source, '-o', output])
                        match = re.search(rb'CONSTANT_WORK expansions=(\d+)', result.stderr)
                        self.assertIsNotNone(match, result.stderr.decode())
                        self.assertLessEqual(int(match[1]), 32 * (depth + 1),
                                             'repeated aliases were expanded as a tree')
                        self.assertEqual(output.read_bytes(), expected)

    def test_constant_cache_allocation_failure_preserves_output(self):
        with tempfile.TemporaryDirectory(prefix='xasm-constant-failure-') as directory:
            root = Path(directory)
            source, output = root / 'input.asm', root / 'output.bin'
            source.write_text('.byte Value\nValue .equ Alias+Alias\nAlias=1\n')
            output.write_bytes(b'previous output')
            result = subprocess.run([str(self.executable), '--pure-binary', str(source), '-o', str(output)],
                                    env={**os.environ, 'XASM_TEST_CONSTANT_CACHE_FAIL': '1'},
                                    capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 1, result.stderr.decode())
            self.assertEqual(result.stderr.count(b'out of memory resolving constants'), 1,
                             result.stderr.decode())
            self.assertEqual(output.read_bytes(), b'previous output')

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
