#!/usr/bin/env python3
"""Run malformed inputs and allocation failures through the actual tools."""
import json
import os
from pathlib import Path
import resource
import re
import subprocess
import tempfile
import unittest

from tool_test_support import build_tool, object_file, SANITIZER_REPORT


class InputHandling(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory(prefix='xorcyst-input-build-')
        cls.addClassCleanup(build.cleanup)
        cls.tools = {name: Path(build.name) / name for name in ('xasm', 'xlnk')}
        build_tool('xasm', cls.tools['xasm'], 'TEST_INPUT_CFLAGS',
                   {'symtab.c': 'tests/test_symtab_alloc.c', 'astnode.c': 'tests/test_astnode_alloc.c',
                    'listing.c': 'tests/test_listing_values.c'})
        build_tool('xlnk', cls.tools['xlnk'], 'TEST_INPUT_CFLAGS',
                   {'unit.c': 'tests/test_unit_alloc.c', 'hashtab.c': None})

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='xorcyst-input-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def run_tool(self, name, content, environment=None, arguments=(), limited=False,
                 pure_binary=True, timeout=20):
        source = 'input.asm' if name == 'xasm' else 'input.script'
        (self.root / source).write_bytes(content.encode() if isinstance(content, str) else content)
        (self.root / 'output').write_bytes(b'previous output')
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(('XASM_TEST_', 'XLNK_TEST_'))}
        env.update(environment or {})
        def limit_resources():
            # Malformed storage counts must not generate huge files or loops
            # if the range check regresses.
            resource.setrlimit(resource.RLIMIT_FSIZE, (65536, 65536))
            resource.setrlimit(resource.RLIMIT_CPU, (1, 1))
        result = subprocess.run([str(self.tools[name]), source, '-o', 'output',
                                 *(['--pure-binary'] if name == 'xasm' and pure_binary else []), *arguments],
                                cwd=self.root, env=env, capture_output=True, timeout=timeout,
                                preexec_fn=limit_resources if limited else None)
        self.assertGreaterEqual(result.returncode, 0, result.stderr.decode())
        self.assertNotRegex(result.stderr, SANITIZER_REPORT)
        if result.returncode:
            self.assertEqual((self.root / 'output').read_bytes(), b'previous output')
        self.assertEqual(list(self.root.glob('.xasm-*')), [])
        self.assertFalse((self.root / 'output.tmp').exists())
        return result

    def test_failed_parser_root_allocation_is_an_error_and_frees_statements(self):
        for source in ('\n', '.ORG $8000\nEntry:\nLDA #$42\n.DB "payload"\nRTS\n'):
            with self.subTest(source=source):
                result = self.run_tool('xasm', source, {'XASM_TEST_LIST_ALLOC_FAIL': '1'})
                self.assertNotEqual(result.returncode, 0, result.stderr.decode())
                self.assertIn(b'INJECT_LIST_ALLOC', result.stderr)
                self.assertIn(b'memory exhausted', result.stderr)

    def test_malformed_source_is_rejected_without_crashes_or_leaks(self):
        cases = ('LDA #(', 'PROC Incomplete\nNOP\n', 'MACRO Incomplete\nNOP\n',
                 'IF 1\nLDA #1\n', '.DB 1,\n', 'Value .EQU 1\nLDA Value::Missing\n',
                 'RECORD Broken field:0\n', 'RECORD Broken field:Missing\n',
                 'RECORD Broken field:1,field:2\n',
                 '.ORG $8000\n.DSB -1\n', 'LDA #(' + '(' * 11000 + '1\n')
        for source in cases:
            with self.subTest(source=source[:60]):
                result = self.run_tool('xasm', source)
                self.assertNotEqual(result.returncode, 0, result.stderr.decode())

    def test_data_label_sort_preserves_zero_page_alignment_and_size_priority(self):
        fields = [('zp_byte', 1, True, 1, 0x21), ('zp_wide', 3, True, 1, 0x22),
                  ('zp_aligned', 4, True, 16, 0x30), ('plain_small', 2, False, 1, 0x25),
                  ('plain_large', 7, False, 1, 0x27), ('plain_aligned', 5, False, 64, 0x40)]
        expected = b''.join(address.to_bytes(2, 'little') for _, _, _, _, address in fields)
        for reverse in (False, True):
            ordered = list(reversed(fields)) if reverse else fields
            for rotation in range(len(fields)):
                order = ordered[rotation:] + ordered[:rotation]
                for debug in (False, True):
                    with self.subTest(reverse=reverse, rotation=rotation, debug=debug):
                        source = '.dataseg\n'
                        for name, size, zp, alignment, _ in order:
                            source += ('.zeropage ' if zp else '') + f'{name} .byte[{size}]\n'
                            if alignment > 1:
                                source += f'.align {name} {alignment}\n'
                        source += '.codeseg\n.word ' + ','.join(f[0] for f in fields) + '\n'
                        result = self.run_tool('xasm', source, pure_binary=False,
                                               arguments=('--debug',) if debug else ())
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        self.assertEqual(result.stderr, b'')
                        (self.root / 'output').rename(self.root / 'unit.o')
                        result = self.run_tool('xlnk', 'ram{start=$21,end=$7FF}\n'
                                               'link{file=unit.o,origin=$8000}\n')
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        self.assertEqual(result.stderr, b'')
                        self.assertEqual((self.root / 'output').read_bytes(), expected)

    def test_constant_reassignments_release_old_definitions_and_keep_captured_values(self):
        body = ('Value=1\nlda #Value\nValue=Value+2\nlda #Value\n'
                'Text="old"\n.db Text\nText="new"\n.db Text\n'
                'Addr=Later\n.word Addr\nAddr=Later+1\n.word Addr\n'
                'Index=0\n.rept 32\n.db Index\nIndex=Index+1\n.endm\n'
                'Later: rts\n')
        expected = bytes.fromhex('A9 01 A9 03') + b'oldnew' + bytes.fromhex('2E 80 2F 80')
        expected += bytes(range(32)) + b'\x60'
        for pure in (False, True):
            for debug in (False, True):
                for analysis in (False, True):
                    with self.subTest(pure=pure, debug=debug, analysis=analysis):
                        flags = ['--debug'] if debug else []
                        stem = f'reassign-{pure}-{debug}-{analysis}'
                        if analysis:
                            flags += [f'--listing={stem}.json', '--listing-format=json',
                                      f'--xref={stem}.xref', '--xref-data=true']
                            if pure:
                                flags += ['--xref-instructions=true',
                                          f'--instruction-records-output={stem}.records',
                                          f'--dependency-manifest={stem}.deps']
                        result = self.run_tool('xasm', ('.org $8000\n' if pure else '') + body,
                                               {'XASM_TEST_TRACK_NODES': '1'}, pure_binary=pure, arguments=flags)
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        self.assertEqual(result.stderr, b'AST_NODES_LIVE=0\n')
                        if not pure:
                            (self.root / 'output').rename(self.root / 'unit.o')
                            result = self.run_tool('xlnk', 'link{file=unit.o,origin=$8000}\n')
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                        self.assertEqual((self.root / 'output').read_bytes(), expected)
                        if pure and analysis:
                            records = json.loads((self.root / (stem + '.records')).read_text())['records']
                            self.assertEqual([r['operand_value'] for r in records], [1, 3, None])
                            self.assertNotEqual(records[0]['additive_terms'], records[1]['additive_terms'])

    def test_constant_reassignment_cleanup_after_error_and_undef(self):
        for definition in ('Value=1', 'Value .equ 1', '.define Value 1'):
            for fail in (False, True):
                with self.subTest(definition=definition, fail=fail):
                    source = (definition + '\nValue=2\n.db Value\n.undef Value\n'
                              'Value=3\nValue=4\n.db Value\n')
                    if fail:
                        source += '.error "stop after reassignment"\n'
                    result = self.run_tool('xasm', source, {'XASM_TEST_TRACK_NODES': '1'})
                    self.assertEqual(result.returncode, int(fail), result.stderr.decode())
                    self.assertIn(b'AST_NODES_LIVE=0\n', result.stderr)
                    if fail:
                        self.assertIn(b'stop after reassignment', result.stderr)
                    else:
                        self.assertEqual((self.root / 'output').read_bytes(), b'\x02\x04')

    def test_reassignment_does_not_free_borrowed_symbol_definitions(self):
        # Compatibility only: assignment to a non-constant is a tracked defect.
        # Those paths already leak; keep ASan's access checks, but not LSan here.
        environment = {'ASAN_OPTIONS': os.environ.get('ASAN_OPTIONS', '') + ':detect_leaks=0'}
        cases = [('L: nop\nL = 5\nlda #L\n', 0, b'', bytes.fromhex('EA A9 00')),
                 ('.proc P\nrts\n.endp\nP = 3\njsr P\n', 1,
                  b'input.asm:5: error: operand does not evaluate to literal\n', None)]
        for source, status, diagnostic, expected in cases:
            for debug in (False, True):
                for analysis in (False, True):
                    with self.subTest(source=source, debug=debug, analysis=analysis):
                        flags = ['--debug'] if debug else []
                        if analysis:
                            flags += ['--xref=borrowed.json', '--xref-instructions=true']
                        result = self.run_tool('xasm', source, environment, arguments=flags)
                        self.assertEqual(result.returncode, status, result.stderr.decode())
                        self.assertEqual(result.stderr, diagnostic)
                        if expected is not None:
                            self.assertEqual((self.root / 'output').read_bytes(), expected)

    def test_reassignment_preserves_first_definition_and_current_binding_locations(self):
        source = '.org $8000\nV .equ 1\nlda #V\nV = 2\nlda #V\n'
        for debug in (False, True):
            with self.subTest(debug=debug):
                xref = self.root / f'locations-{debug}.json'
                flags = [f'--xref={xref}', '--xref-instructions=true']
                if debug:
                    flags += ['--debug']
                result = self.run_tool('xasm', source, {'XASM_TEST_TRACK_NODES': '1'}, arguments=flags)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual(result.stderr, b'AST_NODES_LIVE=0\n')
                self.assertEqual((self.root / 'output').read_bytes(), bytes.fromhex('A9 01 A9 02'))
                facts = json.loads(xref.read_text())
                symbol = next(symbol for symbol in facts['symbols'] if symbol['name'] == 'V')
                self.assertEqual(symbol['definition']['line'], 2)
                self.assertEqual(symbol['definition']['value'], 2)
                records = facts['instruction_records']['records']
                self.assertEqual([record['operand_value'] for record in records], [1, 2])
                self.assertEqual([record['additive_terms']['terms'][0]['binding']['definition']['line']
                                  for record in records], [2, 4])

    def test_forward_label_identities_wait_for_addresses(self):
        expressions = ('Fwd+(L2-L1-1)', '(L2-L1-1)+Fwd', 'Fwd*(L2-L1)',
                       '(L2-L1)*Fwd', 'Fwd/(L2-L1)',
                       '(Fwd+(L2-L1-1))*(L2-L1)')
        cases = [(expression, 'lda') for expression in expressions]
        # Late storage sizes also evaluate identities before forward addresses exist.
        cases += [('(Tail-Head)+(L2-L1-1)', '.dsb'),
                  ('(Tail-Head)*(L2-L1)', '.dsb')]
        for index, (expression, directive) in enumerate(cases):
            for origin in (0, 0x8000):
                # Forward storage sizes are supported here only in pure-binary mode.
                for pure in ((True,) if directive == '.dsb' else (False, True)):
                    for debug in (False, True):
                        for analysis in (False, True):
                            with self.subTest(expression=expression, directive=directive, origin=origin, pure=pure,
                                              debug=debug, analysis=analysis):
                                stem = f'ident-{index}-{origin}-{pure}-{debug}-{analysis}'
                                listing = self.root / (stem + '.json')
                                flags = ['--debug'] if debug else []
                                if analysis:
                                    flags += [f'--listing={listing}', '--listing-format=json',
                                              '--listing-version=' + ('2' if pure else '1'),
                                              f'--xref={stem}.xref']
                                    if pure:
                                        flags += ['--xref-instructions=true', '--xref-data=true',
                                                  f'--instruction-records-output={stem}.records',
                                                  f'--dependency-manifest={stem}.deps']
                                source = (f'.org ${origin:04X}\n' if pure else '')
                                source += f'L1: nop\nL2: {directive} {expression}\n'
                                source += ('.byte 1\nHead: nop\nnop\nTail: rts\n' if directive == '.dsb'
                                           else 'Fwd: rts\n')
                                result = self.run_tool('xasm', source, pure_binary=pure, arguments=flags)
                                self.assertEqual(result.returncode, 0, result.stderr.decode())
                                self.assertEqual(result.stderr, b'')
                                if not pure:
                                    (self.root / 'output').rename(self.root / 'unit.o')
                                    result = self.run_tool('xlnk', f'link{{file=unit.o,origin=${origin:04X}}}\n')
                                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                                expected = (bytes.fromhex('EA A5 03 60') if origin == 0
                                            else bytes((0xEA, 0xAD, 4, origin >> 8, 0x60)))
                                if directive == '.dsb':
                                    expected = bytes.fromhex('EA 00 00 01 EA EA 60')
                                self.assertEqual((self.root / 'output').read_bytes(), expected)
                                # Late storage listing extents have a separate tracked limitation.
                                if pure and analysis and directive != '.dsb':
                                    rows = json.loads(listing.read_text())['records']
                                    self.assertEqual(b''.join(bytes.fromhex(''.join(r['bytes_hex']))
                                                             for r in rows), expected)
                                    records = json.loads((self.root / (stem + '.records')).read_text())['records']
                                    self.assertEqual(records[1]['operand_value'], origin + (3 if origin == 0 else 4))

    def test_invalid_shifts_with_forward_labels_fail_without_crashing(self):
        for count in (-1, 32, 63, 64):
            for expression in (f'Fwd+((L2-L1)>>({count}))',
                               f'((L2-L1)>>({count}))+Fwd',
                               f'Fwd*(1+((L2-L1)>>({count})))',
                               f'(1+((L2-L1)>>({count})))*Fwd',
                               f'Fwd/(1+((L2-L1)>>({count})))'):
                for debug in (False, True):
                    for analysis in (False, True):
                        with self.subTest(count=count, expression=expression, debug=debug, analysis=analysis):
                            flags = ['--debug'] if debug else []
                            if analysis:
                                flags += ['--listing=listing.json', '--listing-format=json', '--listing-version=2',
                                          '--xref=xref.json', '--xref-instructions=true', '--xref-data=true',
                                          '--instruction-records-output=records.json', '--dependency-manifest=deps.json',
                                          '--fceux-nl-ram-output=ram.nl', '--fceux-nl-rom-prefix=rom.']
                            for filename in ('xref.json', 'records.json', 'deps.json', 'ram.nl', 'rom.0.nl'):
                                (self.root / filename).write_bytes(b'previous analysis')
                            listing = self.root / 'listing.json'
                            listing.unlink(missing_ok=True)
                            source = f'.org $8000\nL1: nop\nL2: lda {expression}\nFwd: rts\n'
                            result = self.run_tool('xasm', source, arguments=flags)
                            self.assertEqual(result.returncode, 1, result.stderr.decode())
                            self.assertIn(b'input.asm:3: error: invalid shift count in expression', result.stderr)
                            if analysis:
                                rows = json.loads(listing.read_text())['records']
                                self.assertTrue(rows)
                                self.assertTrue(all(not r['binary_generated'] and not r['emits_bytes']
                                                    and not r['bytes_hex'] for r in rows))
                            for filename in ('xref.json', 'records.json', 'deps.json', 'ram.nl', 'rom.0.nl'):
                                self.assertEqual((self.root / filename).read_bytes(), b'previous analysis')

    def test_empty_initializer_expansions_keep_processing_after_errors(self):
        cases = (
            ('.struc Empty\n.ends\n', '.Empty {1}\n', b'too many field initializers'),
            ('.record Flags padding:1, enabled:1, index:6\ninit .equ {0,1,5}\n',
             '.Flags init\n', b'record initializer expected'),
            ('.struc Point\npx .byte\npy .byte\n.ends\n',
             '.Point 1\n', b'structure initializer expected'),
            ('.union Choice\nsmall .byte\nwide .word\n.ends\n',
             '.Choice 1\n', b'union initializer expected'),
        )
        for declaration, statement, diagnostic in cases:
            for body in (statement * 2, '.macro Emit\n' + statement + '.endm\nEmit\nEmit\n',
                         '.rept 2\n' + statement + '.endm\n'):
                for pure_binary in (False, True):
                    for listing in (False, True):
                        with self.subTest(statement=statement, body=body,
                                          pure_binary=pure_binary, listing=listing):
                            arguments = ('--listing=listing.json', '--listing-format=json',
                                         '--xref=xref.json') if listing else ()
                            result = self.run_tool('xasm', declaration + body,
                                                   arguments=arguments, pure_binary=pure_binary)
                            self.assertEqual(result.returncode, 1, result.stderr.decode())
                            self.assertEqual(result.stderr.count(diagnostic), 2, result.stderr.decode())

    def test_unsupported_object_expressions_never_publish_output(self):
        types = ('.struc S\nfa .byte\nfb .byte\n.ends\n'
                 '.dataseg\ninst .S\narr .S[4]\n.codeseg\n')
        cases = (
            ('.struc P\nitems .byte[3]\n.ends\n.P {{{1,2,3}}}\n', b'STRUC_NODE'),
            ('Begin:\n@@s: .byte 1\n@@e: lda #Alias\nAlias .equ @@e-@@s\n', b'LOCAL_ID_NODE'),
            (types + 'lda arr[inst.fa].fb\n', b'DOT_NODE'),
            (types + '.byte arr[inst.missing].fa\n', b'DOT_NODE'),
        )
        for source, node_type in cases:
            for debug in (False, True):
                for analysis in ((), ('--listing=listing.json', '--listing-format=json'),
                                 ('--xref=xref.json',)):
                    with self.subTest(source=source, debug=debug, analysis=analysis):
                        result = self.run_tool('xasm', source, pure_binary=False,
                                               arguments=analysis + (('--debug',) if debug else ()))
                        self.assertEqual(result.returncode, 1, result.stderr.decode())
                        self.assertIn(b'error: unsupported expression in object output', result.stderr)
                        self.assertIn(node_type, result.stderr)
                        self.assertRegex(result.stderr, rb'input\.asm:\d+: error:')
                        # Also reject it when no prior output exists.
                        (self.root / 'output').unlink()
                        result = subprocess.run([str(self.tools['xasm']), 'input.asm', '-o', 'output',
                                                 *analysis, *(('--debug',) if debug else ())],
                                                cwd=self.root, capture_output=True, timeout=20)
                        self.assertEqual(result.returncode, 1, result.stderr.decode())
                        self.assertNotRegex(result.stderr, SANITIZER_REPORT)
                        self.assertFalse((self.root / 'output').exists())
                        self.assertFalse((self.root / 'output.tmp').exists())

    def test_object_expression_limit_preserves_output(self):
        source = 'Target: nop\n' + ''.join(f'.word Target+{i}\n' for i in range(16390))
        for debug in (False, True):
            with self.subTest(debug=debug):
                flags = ('--debug',) if debug else ()
                result = self.run_tool('xasm', source, pure_binary=False, arguments=flags, timeout=120)
                self.assertEqual(result.returncode, 1, result.stderr.decode())
                self.assertEqual(result.stderr.count(b'too many object expressions (maximum 16384)'), 1)
                self.assertRegex(result.stderr, rb'input\.asm:\d+: error:')
                (self.root / 'output').unlink()
                result = subprocess.run([str(self.tools['xasm']), 'input.asm', '-o', 'output', *flags],
                                        cwd=self.root, capture_output=True, timeout=120)
                self.assertEqual(result.returncode, 1, result.stderr.decode())
                self.assertNotRegex(result.stderr, SANITIZER_REPORT)
                self.assertEqual(result.stderr.count(b'too many object expressions (maximum 16384)'), 1)
                self.assertFalse((self.root / 'output').exists())
                self.assertFalse((self.root / 'output.tmp').exists())

    def test_object_expression_limit_allows_boundary_and_duplicates(self):
        # Exactly 16384 distinct expressions, then duplicates after the table
        # is full. They must reuse IDs and still link to the correct values.
        values = [*range(16384), 0, 1, 16383]
        source = 'Target: nop\n' + ''.join(f'.word Target+{i}\n' for i in values)
        for debug in (False, True):
            with self.subTest(debug=debug):
                result = self.run_tool('xasm', source, pure_binary=False,
                                       arguments=('--debug',) if debug else (), timeout=120)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual(result.stderr, b'')
                (self.root / 'link.script').write_text('link{file=output,origin=0}\n')
                linked = self.root / 'linked.bin'
                linked.unlink(missing_ok=True)
                result = subprocess.run([str(self.tools['xlnk']), 'link.script', '-o', linked.name],
                                        cwd=self.root, capture_output=True, timeout=20)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertNotRegex(result.stderr, SANITIZER_REPORT)
                self.assertEqual(linked.read_bytes(), b'\xea' + b''.join(
                    value.to_bytes(2, 'little') for value in values))

    def test_failed_instruction_allocation_frees_operand_and_prior_statements(self):
        for instruction in ('NOP', 'ASL A', 'LDA #Port+2', 'LDA Port+2',
                            'LDA Port+2,X', 'LDA Port+2,Y', 'LDA [Port+2,X]',
                            'LDA [Port+2],Y', 'JMP [Port+2]'):
            with self.subTest(instruction=instruction):
                source = ('Port .EQU $10\n.ORG $8000\nEntry:\n.DB "payload"\nNOP\n'
                          + instruction + '\nRTS\nEND\n')
                result = self.run_tool('xasm', source)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                result = self.run_tool('xasm', source, {'XASM_TEST_INSTRUCTION_ALLOC_FAIL': '1'})
                self.assertNotEqual(result.returncode, 0, result.stderr.decode())
                self.assertIn(b'INJECT_INSTRUCTION_ALLOC', result.stderr)
                self.assertIn(b'memory exhausted', result.stderr)

    def test_excessive_macro_and_include_nesting_is_rejected(self):
        for source in ('MACRO Again\nAgain\nENDM\nAgain\n', '.INCSRC "input.asm"\n'):
            with self.subTest(source=source):
                result = self.run_tool('xasm', source)
                self.assertNotEqual(result.returncode, 0, result.stderr.decode())
                self.assertRegex(result.stderr, rb'nesting|depth')

    def test_deep_valid_initializers_and_failed_scope_growth(self):
        source = 'STRUC Type0\nValue BYTE\nENDS\n'
        source += ''.join(f'STRUC Type{i}\nChild . Type{i-1}\nENDS\n' for i in range(1, 65))
        source += '.ORG $8000\n. Type64 ' + '{' * 65 + '$42' + '}' * 65 + '\nEND\n'
        result = self.run_tool('xasm', source)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual((self.root / 'output').read_bytes(), b'B')
        for index in (1, 2):
            result = self.run_tool('xasm', source, {'XASM_TEST_SYMTAB_FUNCTION': 'symtab_push',
                                                  'XASM_TEST_SYMTAB_FAIL': str(index)})
            self.assertNotEqual(result.returncode, 0, result.stderr.decode())
            self.assertIn(b'INJECT_SYMTAB', result.stderr)
            self.assertIn(b'out of memory', result.stderr)

    def test_duplicate_symbols_still_report_the_actual_error(self):
        cases = ('MACRO Again\nNOP\nENDM\n' * 2,
                 'STRUC Pair\nField BYTE\nField BYTE\nENDS\n',
                 'UNION Pair\nField BYTE\nField WORD\nENDS\n',
                 'ENUM Values\nOne\nOne\nENDE\n',
                 'RECORD Bits field:1,field:2\n')
        for source in cases:
            with self.subTest(source=source):
                result = self.run_tool('xasm', source)
                self.assertNotEqual(result.returncode, 0, result.stderr.decode())
                self.assertIn(b'duplicate symbol', result.stderr)
                self.assertNotIn(b'out of memory', result.stderr)

    def test_symbol_allocation_failures_reach_the_cli_without_partial_outputs(self):
        source = ('STRUC Pair\nFirst BYTE\nSecond BYTE\nENDS\n'
                  'UNION Either\nByteValue BYTE\nWordValue WORD\nENDS\n'
                  'ENUM Values\nOne\nTwo\nENDE\nRECORD Bits first:1,rest:7\n'
                  'Port .EQU $10\nMACRO Load\nLDA Port\nENDM\n'
                  'DATASEG\nBuffer BYTE\nCODESEG\n.ORG $8000\nEntry:\nLoad\nRTS\nEND\n')
        self.check_symbol_allocation_failures(source)

    def test_anonymous_union_allocation_failures_preserve_ast_ownership(self):
        # Sweep both the anonymous symbol's insertion and its nested scope
        # creation. Linux LeakSanitizer also checks cleanup of the attached AST.
        source = ('STRUC Container\nBefore BYTE\nUNION\n'
                  'ByteValue BYTE\nWordValue WORD\nENDS\nAfter BYTE\nENDS\n'
                  '.ORG $8000\n.DB sizeof(Container)\nEND\n')
        result = self.run_tool('xasm', source)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual((self.root / 'output').read_bytes(), b'\x04')
        self.check_symbol_allocation_failures(source)

    def check_symbol_allocation_failures(self, source):
        for function in ('symtab_create', 'symtab_enter', 'symtab_push'):
            for index in range(100):
                with self.subTest(function=function, index=index):
                    result = self.run_tool('xasm', source, {'XASM_TEST_SYMTAB_FUNCTION': function,
                                                         'XASM_TEST_SYMTAB_FAIL': str(index)})
                    if b'INJECT_SYMTAB' not in result.stderr:
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        self.assertGreater(index, 0)
                        break
                    self.assertNotEqual(result.returncode, 0, result.stderr.decode())
                    self.assertIn(b'out of memory', result.stderr)
                    self.assertNotIn(b'duplicate symbol', result.stderr)
            else:
                self.fail('symbol allocation sweep did not reach the end')

    def test_malformed_linker_scripts_are_rejected_without_hanging(self):
        for source in ('output{file=output\n', 'copy{file}\n', 'bank{size=}\n',
                       'options{unknown=1}\n', 'output{file=' + 'a' * 512 + '}\n',
                       'copy{file=unterminated\\', 'pad{size=1 unknown=2}\n', b'pad{size=1}\0bad\n'):
            with self.subTest(source=str(source)[:60]):
                result = self.run_tool('xlnk', source)
                self.assertNotEqual(result.returncode, 0, result.stderr.decode())

    def check_bad_object(self, data):
        (self.root / 'unit.o').write_bytes(data)
        result = self.run_tool('xlnk', 'link{file=unit.o,origin=$8000}\n')
        self.assertNotEqual(result.returncode, 0, result.stderr.decode())
        return result

    def test_script_trailing_text_is_rejected_before_any_output(self):
        for suffix in ('garbage', 'pad{size=1}', '\rgarbage', '} # comment'):
            with self.subTest(suffix=suffix):
                result = self.run_tool('xlnk', 'output{file=output} ' + suffix + '\npad{size=3}\n')
                self.assertNotEqual(result.returncode, 0, result.stderr.decode())
                self.assertIn(b'unexpected text after command', result.stderr)

    def test_script_trailing_whitespace_comments_and_crlf_are_accepted(self):
        for ending in ('', '\n', '\r\n'):
            for suffix in ('', ' \t', '# comment', ' \t# comment'):
                with self.subTest(ending=ending, suffix=suffix):
                    result = self.run_tool('xlnk', 'pad{size=3}' + suffix + ending)
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    self.assertEqual((self.root / 'output').read_bytes(), b'\0' * 3)

    def test_segment_trailing_bytes_are_rejected(self):
        for segment in ('data', 'code'):
            for trailing in (b'\xff', b'\0', b'\xf3', bytes.fromhex('f400eaf3')):
                with self.subTest(segment=segment, trailing=trailing):
                    result = self.check_bad_object(object_file(**{segment: b'\xf3' + trailing}))
                    self.assertIn(b'unexpected bytes after segment terminator', result.stderr)

    def test_segment_terminator_inside_binary_payload_is_data(self):
        (self.root / 'unit.o').write_bytes(object_file(code=bytes.fromhex('f402f3ff00f3')))
        result = self.run_tool('xlnk', 'link{file=unit.o,origin=$8000}\n')
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual((self.root / 'output').read_bytes(), bytes.fromhex('f3ff00'))

    def test_trailing_object_bytes_are_rejected(self):
        for valid in (object_file(), self.object_with_metadata()):
            for trailing in (b'\0', b'garbage', object_file()):
                with self.subTest(trailing=trailing):
                    result = self.check_bad_object(valid + trailing)
                    self.assertIn(b'unexpected bytes after object expressions', result.stderr)

    def test_every_truncated_prefix_of_an_object_is_rejected(self):
        objects = [object_file(code=bytes.fromhex('f80000f3'), expressions=[bytes.fromhex('0101')]),
                   self.object_with_metadata()]
        for valid in objects:
            for size in range(len(valid)):
                with self.subTest(length=len(valid), size=size):
                    self.check_bad_object(valid[:size])

    def test_bad_object_references_bytecodes_and_expression_depth_are_rejected(self):
        cases = [object_file(code=bytes.fromhex(code)) for code in
                 ('ff', 'f600', 'f601ff41', 'f4ff00f3', 'f8fffff3')]
        cases += [object_file(code=bytes.fromhex('f7020000f3'), expressions=[bytes.fromhex('0100')])]
        cases += [object_file(code=bytes.fromhex('f80000f3'), expressions=[expr]) for expr in
                  (bytes.fromhex('08ffff'), bytes.fromhex('07ffff'), bytes.fromhex('1301010100'),
                   bytes.fromhex('15010101ff'), b'\x24' * 501 + bytes.fromhex('0100'))]
        for index, data in enumerate(cases):
            with self.subTest(index=index):
                self.check_bad_object(data)

    def test_instruction_operand_types_are_checked_before_any_output(self):
        # A string, concatenated strings, and an undefined bank-of-integer
        # operation are structurally valid expressions but invalid operands.
        expressions = [bytes.fromhex(value) for value in
                       ('050058', '10050058050059', '250101')]
        for command in (0xF7, 0xFE):
            for expression in expressions:
                with self.subTest(command=command, expression=expression):
                    code = bytes([command, 0xA9, 0, 0, 0xF3])
                    (self.root / 'unit.o').write_bytes(object_file(code=code, expressions=[expression]))
                    (self.root / 'second').write_bytes(b'previous second')
                    result = self.run_tool('xlnk', 'pad{size=1}\noutput{file=second}\n'
                                           'link{file=unit.o,origin=$8000}\n')
                    self.assertNotEqual(result.returncode, 0, result.stderr.decode())
                    self.assertIn(b'instruction operand', result.stderr)
                    self.assertEqual((self.root / 'second').read_bytes(), b'previous second')

    def test_integer_operands_from_strings_and_forward_labels_are_valid(self):
        for command in (0xF7, 0xFE):
            for opcode, suffix, expression, expected in (
                    (0xA9, b'', bytes.fromhex('1a050058050058'), bytes.fromhex('a901')),
                    (0xAD, bytes.fromhex('f600'), bytes.fromhex('080000'), bytes.fromhex('ad0380'))):
                with self.subTest(command=command, opcode=opcode):
                    code = bytes([command, opcode, 0, 0]) + suffix + b'\xf3'
                    (self.root / 'unit.o').write_bytes(object_file(code=code, expressions=[expression]))
                    result = self.run_tool('xlnk', 'link{file=unit.o,origin=$8000}\n')
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    self.assertEqual((self.root / 'output').read_bytes(), expected)

    def test_linker_zero_page_layout_updates_forward_and_backward_labels(self):
        for origin in (0, 0x80, 0xF0, 0x100, 0x8000):
            for debug in (False, True):
                for wide in (False, True):
                    with self.subTest(origin=origin, debug=debug, wide=wide):
                        mnemonic = 'lda.w' if wide else 'lda'
                        source = f'Start: {mnemonic} Target\nbne Start\nTarget: nop\n.word Start,Target\n'
                        result = self.run_tool('xasm', source, pure_binary=False,
                                               arguments=('--debug',) if debug else ())
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        (self.root / 'output').rename(self.root / 'unit.o')
                        result = self.run_tool('xlnk', f'link{{file=unit.o,origin={origin}}}\n')
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        short = origin < 0x100 and not wide
                        length = 2 if short else 3
                        end = origin + length + 2
                        expected = (bytes([0xA5 if short else 0xAD])
                                    + end.to_bytes(1 if short else 2, 'little')
                                    + bytes([0xD0, 256-length-2, 0xEA])
                                    + origin.to_bytes(2, 'little') + end.to_bytes(2, 'little'))
                        self.assertEqual((self.root / 'output').read_bytes(), expected)

    def test_linker_zero_page_boundary_requires_repeated_layout(self):
        # Far initially lies at $100. Shortening the second LDA brings it to
        # $FF; the first LDA can only shorten on the next relocation pass.
        source = 'lda Far\nlda Near\nNear: .byte 0\nFar: nop\n.word Near,Far\n'
        for debug in (False, True):
            result = self.run_tool('xasm', source, pure_binary=False,
                                   arguments=('--debug',) if debug else ())
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            (self.root / 'output').rename(self.root / 'unit.o')
            result = self.run_tool('xlnk', 'link{file=unit.o,origin=$F9}\n')
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertEqual((self.root / 'output').read_bytes(),
                             bytes.fromhex('A5 FE A5 FD 00 EA FD 00 FE 00'))

    def test_linker_zero_page_relaxation_relocates_following_units(self):
        for bank in (False, True):
            for debug in (False, True):
                with self.subTest(bank=bank, debug=debug):
                    for name, source in (
                        ('consumer.o', '.extrn Target:label\nlda Target\njmp Target\n'),
                        ('provider.o', '.public Target\nTarget: nop\n.word Target\n')):
                        result = self.run_tool('xasm', source, pure_binary=False,
                                               arguments=('--debug',) if debug else ())
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        (self.root / 'output').rename(self.root / name)
                    script = 'bank{size=32,origin=0}\n' if bank else ''
                    script += 'link{file=consumer.o}\nlink{file=provider.o}\n'
                    if bank: script += 'pad{offset=12}\n'
                    result = self.run_tool('xlnk', script)
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    expected = bytes.fromhex('A5 05 4C 05 00 EA 05 00')
                    if bank: expected += bytes(24)  # Remaining bank space is padded too.
                    self.assertEqual((self.root / 'output').read_bytes(), expected)

    def test_linker_fixed_address_labels_survive_relaxation(self):
        # LDA fixed; fixed @ $A0: NOP; DW fixed. A fixed address is not the
        # current output position and must not be overwritten by layout.
        code = bytes.fromhex('f7ad0000f60800a0f400eaf90000f3')
        (self.root / 'unit.o').write_bytes(object_file(code=code, expressions=[bytes.fromhex('080000')]))
        result = self.run_tool('xlnk', 'link{file=unit.o,origin=0}\n')
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual((self.root / 'output').read_bytes(), bytes.fromhex('A5 A0 EA A0 00'))

    def test_linker_relaxes_only_unsigned_byte_addresses(self):
        for expression, expected in (('240101', 'AD FF FF'), ('0100', 'A5 00'),
                                     ('01ff', 'A5 FF'), ('020100', 'AD 00 01')):
            with self.subTest(expression=expression):
                (self.root / 'unit.o').write_bytes(object_file(
                    code=bytes.fromhex('f7ad0000f3'), expressions=[bytes.fromhex(expression)]))
                result = self.run_tool('xlnk', 'link{file=unit.o,origin=0}\n')
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual((self.root / 'output').read_bytes(), bytes.fromhex(expected))

    def test_linker_rejects_operand_that_outgrows_relaxed_mode(self):
        result = self.run_tool('xasm', 'lda $102-Target\nTarget: nop\n', pure_binary=False)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        (self.root / 'output').rename(self.root / 'unit.o')
        result = self.run_tool('xlnk', 'link{file=unit.o,origin=0}\n')
        self.assertEqual(result.returncode, 1, result.stderr.decode())
        self.assertIn(b"instruction operand doesn't fit in 1 byte", result.stderr)

    def test_linker_rejects_negative_final_zero_page_addresses(self):
        for instruction in ('lda Target-3', 'lda Target-3,x', 'ldx Target-3,y'):
            for debug in (False, True):
                with self.subTest(instruction=instruction, debug=debug):
                    result = self.run_tool('xasm', instruction + '\nTarget: nop\n',
                                           pure_binary=False, arguments=('--debug',) if debug else ())
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    (self.root / 'output').rename(self.root / 'unit.o')
                    result = self.run_tool('xlnk', 'link{file=unit.o,origin=0}\n')
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    self.assertIn(b"instruction operand doesn't fit in 1 byte", result.stderr)

    def test_binary_rejects_final_zero_page_operands_outside_address_range(self):
        for mnemonic, suffix in (('lda', ''), ('lda', ',x'), ('ldx', ',y'),
                                 ('sty', ',x'), ('stx', ',y')):
            for operand in ('Target-3', '$102-Target'):
                instruction = f'{mnemonic} {operand}{suffix}\n'
                for body in (instruction, '.macro Emit\n' + instruction + '.endm\nEmit\n',
                             '.rept 1\n' + instruction + '.endm\n'):
                    for debug in (False, True):
                        for analysis in ((), ('--listing=listing.json', '--listing-format=json'),
                                         ('--xref=xref.json',)):
                            with self.subTest(instruction=instruction, body=body, debug=debug,
                                              analysis=analysis):
                                result = self.run_tool('xasm', body + 'Target: nop\n',
                                                       arguments=analysis + (('--debug',) if debug else ()))
                                self.assertEqual(result.returncode, 1, result.stderr.decode())
                                self.assertIn(b"instruction operand doesn't fit in 1 byte", result.stderr)

    def test_binary_zero_page_validation_preserves_other_addressing_modes(self):
        cases = (
            ('lda Target-2\nTarget: nop\n', 'A5 00 EA'),
            ('lda $101-Target\nTarget: nop\n', 'A5 FF EA'),
            ('lda Target+254\nTarget: nop\n', 'AD 01 01 EA'),
            ('lda Target-4\nTarget: nop\n', 'AD FF FF EA'),
            ('lda.w Target-3\nTarget: nop\n', 'AD 00 00 EA'),
            ('lda #Target-3\nTarget: bne Target\n', 'A9 FF D0 FE'),
            ('lda [Target-3],y\nTarget: nop\n', 'B1 FF EA'),
            ('lda [Target-3,x]\nTarget: nop\n', 'A1 FF EA'),
            # Literal signed bytes have already been accepted by translation,
            # matching the raw instruction bytes emitted in object mode.
            ('stx -1,y\nsty -1,x\n', '96 FF 94 FF'),
        )
        for source, expected in cases:
            for debug in (False, True):
                with self.subTest(source=source, debug=debug):
                    result = self.run_tool('xasm', source, arguments=('--debug',) if debug else ())
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    self.assertEqual((self.root / 'output').read_bytes(), bytes.fromhex(expected))

    def test_binary_invalid_zero_page_operand_leaves_no_new_output(self):
        (self.root / 'input.asm').write_text('lda Target-3\nTarget: nop\n')
        result = subprocess.run([str(self.tools['xasm']), '--pure-binary', 'input.asm', '-o', 'output'],
                                cwd=self.root, capture_output=True, timeout=20)
        self.assertEqual(result.returncode, 1, result.stderr.decode())
        self.assertIn(b"instruction operand doesn't fit in 1 byte", result.stderr)
        self.assertNotRegex(result.stderr, SANITIZER_REPORT)
        self.assertFalse((self.root / 'output').exists())
        self.assertFalse((self.root / 'output.tmp').exists())

    def test_linker_keeps_negative_immediates_and_backward_branches(self):
        # An expression-backed immediate exercises write_instr, rather than a
        # byte already folded by xasm. The branch still uses a signed offset.
        code = bytes.fromhex('f7a90000f7d00001f3')
        expressions = [bytes.fromhex('240101'), bytes.fromhex('0100')]
        (self.root / 'unit.o').write_bytes(object_file(code=code, expressions=expressions))
        result = self.run_tool('xlnk', 'link{file=unit.o,origin=0}\n')
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual((self.root / 'output').read_bytes(), bytes.fromhex('A9 FF D0 FC'))

    def test_object_integer_widths_preserve_sign_and_compact_positive_values(self):
        # Exercise existing object bytes directly, including high bits in the
        # compact unsigned forms and both endpoints of the signed 32-bit form.
        for width, values in (
                (1, (0, 127, 128, 255)),
                (2, (256, 32767, 32768, 65535)),
                (3, (65536, 8388607, 8388608, 16777215)),
                (4, (0, 16777216, 0x7FFFFFFF, 0x80000000,
                     0xFFFFFF7F, 0xFFFFFF80, 0xFFFFFFFF))):
            for bits in values:
                with self.subTest(width=width, bits=bits):
                    value = bits - (1 << 32) if width == 4 and bits >= (1 << 31) else bits
                    literal = bytes([width]) + bits.to_bytes(width, 'big')
                    # DD literal; DB literal < 0, literal > 0.
                    expressions = [literal, b'\x1c' + literal + b'\x01\0',
                                   b'\x1d' + literal + b'\x01\0']
                    (self.root / 'unit.o').write_bytes(object_file(
                        code=bytes.fromhex('fa0000f80001f80002f3'), expressions=expressions))
                    result = self.run_tool('xlnk', 'link{file=unit.o,origin=0}\n')
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    self.assertEqual(result.stderr, b'')
                    self.assertEqual((self.root / 'output').read_bytes(),
                                     bits.to_bytes(4, 'little') + bytes([value < 0, value > 0]))

    def test_signed_relocatable_expressions_match_binary_output(self):
        values = (-2147483648, -8388609, -32769, -129, -128, -2, -1, 0,
                  127, 128, 255, 256, 32768, 65536, 8388608, 16777216, 2147483647)
        source = 'L1: nop\nL2:\n'
        expected = bytearray(b'\xea')
        for value in values:
            # Spell INT_MIN without negating an out-of-range positive int.
            literal = '(-2147483647-1)' if value == -2147483648 else str(value)
            expression = f'(L2-L1-1+({literal}))'
            source += f'.dword {expression}\n.byte {expression}<0,{expression}>0\n'
            expected += (value & 0xFFFFFFFF).to_bytes(4, 'little')
            expected += bytes([value < 0, value > 0])
        source += '.word (L2-L1+-4)/2,(L2-L1+-4)%2\nlda #(L2-L1+-2)\n'
        expected += bytes.fromhex('FF FF FF FF A9 FF')
        for origin in (0, 0x8000):
            for debug in (False, True):
                for analysis in ((), ('--listing=listing.json', '--listing-format=json',
                                      '--xref=xref.json')):
                    for pure in (False, True):
                        with self.subTest(origin=origin, debug=debug, analysis=analysis, pure=pure):
                            result = self.run_tool('xasm', (f'.org {origin}\n' if pure else '') + source,
                                                   pure_binary=pure,
                                                   arguments=analysis + (('--debug',) if debug else ()))
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            self.assertEqual(result.stderr, b'')
                            if not pure:
                                (self.root / 'output').rename(self.root / 'unit.o')
                                result = self.run_tool('xlnk', f'link{{file=unit.o,origin={origin}}}\n')
                                self.assertEqual(result.returncode, 0, result.stderr.decode())
                                self.assertEqual(result.stderr, b'')
                            self.assertEqual((self.root / 'output').read_bytes(), expected)

    def test_exported_signed_constants_match_local_constants(self):
        values = (-1, -128, -32768, -2147483648, 128, 32768, 8388608, 2147483647)
        names = ','.join(f'Value{i}' for i in range(len(values)))
        provider = '.public ' + names + '\n'
        consumer = ''
        expected = bytearray()
        for index, value in enumerate(values):
            literal = '(-2147483647-1)' if value == -2147483648 else str(value)
            provider += f'Value{index} .equ {literal}\n'
            consumer += f'.dword Value{index}\n.byte Value{index}<0,Value{index}>0\n'
            expected += (value & 0xFFFFFFFF).to_bytes(4, 'little')
            expected += bytes([value < 0, value > 0])
        consumer += '.dsb 1+(Value0<0)\n.byte $5A\n'
        expected += b'\0\0\x5a'
        for debug in (False, True):
            flags = ('--debug',) if debug else ()
            with self.subTest(debug=debug, local=True):
                result = self.run_tool('xasm', provider + consumer, arguments=flags)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual((self.root / 'output').read_bytes(), expected)
            for name, source in (('provider.o', provider),
                                 ('consumer.o', f'.extrn {names}:byte\n' + consumer)):
                result = self.run_tool('xasm', source, pure_binary=False, arguments=flags)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual(result.stderr, b'')
                (self.root / 'output').rename(self.root / name)
            for order in (('provider.o', 'consumer.o'), ('consumer.o', 'provider.o')):
                with self.subTest(debug=debug, order=order):
                    result = self.run_tool('xlnk', ''.join(f'link{{file={name}}}\n' for name in order))
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    self.assertEqual(result.stderr, b'')
                    self.assertEqual((self.root / 'output').read_bytes(), expected)

    def test_right_shifts_match_for_folded_relocated_and_external_values(self):
        values = (-2147483648, -2, -1, 0, 255, 16777215, 2147483647)
        counts = (0, 1, 16, 24, 25, 31)
        definitions = ''.join(f'Value{i} .equ {value}\n' if value != -2147483648
                              else f'Value{i} .equ $80000000\n'
                              for i, value in enumerate(values))
        names = ','.join(f'Value{i}' for i in range(len(values)))
        for debug in (False, True):
            flags = ('--debug',) if debug else ()
            for form in ('literal', 'constant', 'label', 'external'):
                source, expected = '', bytearray()
                for i, value in enumerate(values):
                    expr = '$80000000' if value == -2147483648 else str(value)
                    if form in ('constant', 'external'):
                        expr = f'Value{i}'
                    elif form == 'label':
                        expr = f'L2-L1-1+({expr})'
                    for count in counts:
                        source += f'.dword ({expr})>>{count}\n.byte (({expr})>>{count})<0\n'
                        bits = (value & 0xFFFFFFFF) >> count
                        expected += bits.to_bytes(4, 'little') + bytes([count == 0 and value < 0])
                # M1's observable instruction/data cases must not warn or fail.
                neg = 'Value1' if form in ('constant', 'external') else (
                    'L2-L1+-3' if form == 'label' else '-2')
                source += (f'lda #({neg})>>25\n.word ({neg})>>16\n'
                           f'.byte ({neg})>>24,({neg})>>31,(({neg})>>1)>>30\n')
                expected += bytes.fromhex('A9 7F FF FF FF 01 01')
                if form == 'constant':
                    source = definitions + source
                elif form == 'label':
                    source = 'L1: nop\nL2:\n' + source
                    expected = b'\xea' + expected
                elif form == 'external':
                    result = self.run_tool('xasm', '.public ' + names + '\n' + definitions,
                                           pure_binary=False, arguments=flags)
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    (self.root / 'output').rename(self.root / 'provider.o')
                    source = f'.extrn {names}:byte\n' + source
                for origin in (0, 0x8000):
                    for pure in ((False,) if form == 'external' else (False, True)):
                        with self.subTest(form=form, debug=debug, origin=origin, pure=pure):
                            result = self.run_tool('xasm', (f'.org {origin}\n' if pure else '') + source,
                                                   pure_binary=pure, arguments=flags)
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            self.assertEqual(result.stderr, b'')
                            if pure:
                                self.assertEqual((self.root / 'output').read_bytes(), expected)
                                continue
                            (self.root / 'output').rename(self.root / 'consumer.o')
                            orders = (('consumer.o',),) if form != 'external' else (
                                ('provider.o', 'consumer.o'), ('consumer.o', 'provider.o'))
                            for order in orders:
                                result = self.run_tool('xlnk', ''.join(
                                    f'link{{file={name}' + (f',origin={origin}' if i == 0 else '') + '}\n'
                                    for i, name in enumerate(order)))
                                self.assertEqual(result.returncode, 0, result.stderr.decode())
                                self.assertEqual(result.stderr, b'')
                                self.assertEqual((self.root / 'output').read_bytes(), expected)

    def test_right_shift_analysis_matches_emitted_bytes(self):
        body = ('L1: nop\nL2: lda #((L1-L2)>>25)\n.dword (L1-L2-1)>>1\n'
                'Half = (L1-L2)>>28\nlda #Half\nlda Half\n'
                'lda Tab+((L1-L2)>>30)\nTab: .byte $11\n'
                '.dword (L1-L2)>>0,(L1-L2+-2147483647)>>31,((L1-L2)>>4)>>27\n'
                'lda #((L1-L2)>>0)\nlda #((L1-L2+-2147483647)>>31)\n'
                'lda #(((L1-L2)>>4)>>27)\n.word Tab+((L1-L2)>>30)\n')
        formats = (('text', 1), ('json', 1), ('ndjson', 1), ('json', 2), ('ndjson', 2),
                   ('xref', 0), ('records', 0))
        for origin in (0x8000, 0xC000):
            expected = (bytes.fromhex('EA A9 7F FF FF FF 7F A9 0F A5 0F AD')
                        + (origin + 17).to_bytes(2, 'little')
                        + bytes.fromhex('11 FF FF FF FF 01 00 00 00 01 00 00 00 A9 FF A9 01 A9 01')
                        + (origin + 17).to_bytes(2, 'little'))
            source = f'.org {origin}\n' + body
            for debug in (False, True):
                flags = ('--debug',) if debug else ()
                plain = self.run_tool('xasm', source, arguments=flags)
                self.assertEqual(plain.returncode, 0, plain.stderr.decode())
                self.assertEqual(plain.stderr, b'')
                self.assertEqual((self.root / 'output').read_bytes(), expected)
                for fmt, version in formats:
                    with self.subTest(origin=origin, debug=debug, format=fmt, version=version):
                        stem = f'{origin}-{debug}-{fmt}-{version}'
                        listing, xref, sidecar = (self.root / (stem + suffix)
                                                   for suffix in ('.lst', '.xref', '.records'))
                        arguments = list(flags)
                        if fmt != 'records':
                            arguments += [f'--xref={xref}', '--xref-instructions=true', '--xref-data=true']
                        if fmt != 'xref':
                            arguments += [f'--instruction-records-output={sidecar}',
                                          f'--dependency-manifest={stem}.deps']
                        if version:
                            arguments += [f'--listing={listing}', f'--listing-format={fmt}',
                                          f'--listing-version={version}']
                        result = self.run_tool('xasm', source, arguments=arguments)
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        self.assertEqual(result.stderr, plain.stderr)
                        self.assertEqual((self.root / 'output').read_bytes(), expected)
                        if fmt == 'text':
                            text = listing.read_text()
                            rows = [line for line in text.splitlines()
                                    if re.match(r'^(?:\d{4}| {4})  [0-9A-F]{4}  ', line)]
                            self.assertEqual(b''.join(bytes.fromhex(line[12:24]) for line in rows), expected)
                            self.assertRegex(text, r'(?m)^Half\s+= \$000F$')
                            self.assertRegex(text, rf'(?m)^Tab\s+= \${origin + 14:04X}$')
                        elif version:
                            rows = (json.loads(listing.read_text())['records'] if fmt == 'json'
                                    else [json.loads(line) for line in listing.read_text().splitlines()])
                            emitted = bytearray()
                            for row in rows:
                                data = bytes.fromhex(''.join(row['bytes_hex']))
                                if not data:
                                    continue
                                offset = row['output_offset_start']
                                self.assertEqual(offset, len(emitted))
                                self.assertEqual(row['output_offset_end'], offset + len(data) - 1)
                                self.assertEqual(int(row['cpu_address_start'], 16), origin + offset)
                                self.assertEqual(data, expected[offset:offset + len(data)])
                                emitted.extend(data)
                            self.assertEqual(emitted, expected)
                        documents = []
                        if fmt != 'records':
                            facts = json.loads(xref.read_text())
                            documents.append(facts['instruction_records'])
                            data_refs = facts['data_directive_references']
                            self.assertEqual([r['use_output_offset'] for r in data_refs], [3, 15, 19, 23, 33])
                            for ref in data_refs:
                                offset, width = ref['use_output_offset'], ref['width_bytes']
                                self.assertEqual(ref['emitted_value'],
                                                 int.from_bytes(expected[offset:offset + width], 'little'))
                        if fmt != 'xref':
                            documents.append(json.loads(sidecar.read_text()))
                        self.assertEqual(documents[0], documents[-1])
                        records = documents[0]['records']
                        self.assertEqual([r['operand_value'] for r in records],
                                         [None, 127, 15, 15, origin + 17, -1, 1, 1])
                        for record in records:
                            offset = record['output_offset']
                            self.assertEqual(bytes(record['bytes']), expected[offset:offset + record['size']])
                            self.assertEqual(record['cpu_address'], origin + offset)
                            if record['operand_value'] is not None:
                                self.assertEqual(sum(t['sign'] * t['value']
                                                     for t in record['additive_terms']['terms']),
                                                 record['operand_value'])
                        self.assertEqual(records[3]['memory_access']['data']['address'], 15)
                        self.assertEqual(records[4]['memory_access']['data']['address'], origin + 17)

    def test_invalid_right_shifts_are_safe_in_diagnostic_listings(self):
        formats = (('text', 1), ('json', 1), ('ndjson', 1), ('json', 2), ('ndjson', 2))
        for count in (-1, 32, 63, 64):
            for index, body in enumerate((f'.byte (L2-L1)>>({count})\n',
                                          f'lda #((L2-L1)>>(L2-L1-1+({count})))\n',
                                          f'Shift = (L2-L1)>>(L2-L1-1+({count}))\n.byte Shift\n',
                                          f'.word L1+((L2-L1)>>({count}))\n',
                                          f'.byte Shift\nShift .equ (L2-L1)>>({count})\n',
                                          f'lda L1+((L2-L1)>>({count}))\n',
                                          f'lda L1+((L2-L1)>>({count})),y\n')):
                source = '.org $8000\nL1: nop\nL2:\n' + body
                for debug in (False, True):
                    flags = ('--debug',) if debug else ()
                    plain = self.run_tool('xasm', source, arguments=flags)
                    self.assertEqual(plain.returncode, 1, plain.stderr.decode())
                    self.assertIn(b'invalid shift count in expression', plain.stderr)
                    self.assertRegex(plain.stderr, rb'input\.asm:\d+: error: invalid shift count')
                    for fmt, version in formats:
                        with self.subTest(count=count, body=index, debug=debug, format=fmt, version=version):
                            stem = f'{count}-{index}-{debug}-{fmt}-{version}'
                            listing, xref, sidecar, ram, manifest = (self.root / (stem + suffix)
                                for suffix in ('.lst', '.xref', '.records', '.nl', '.deps'))
                            exports = (xref, sidecar, ram, manifest)
                            if debug:
                                for path in exports:
                                    path.write_bytes(b'previous analysis')
                            arguments = [*flags, f'--listing={listing}', f'--listing-format={fmt}',
                                         f'--listing-version={version}', f'--xref={xref}',
                                         '--xref-data=true', '--xref-instructions=true',
                                         f'--instruction-records-output={sidecar}',
                                         f'--fceux-nl-ram-output={ram}',
                                         f'--dependency-manifest={manifest}']
                            result = self.run_tool('xasm', source, arguments=arguments)
                            self.assertEqual(result.returncode, 1, result.stderr.decode())
                            self.assertEqual(result.stderr, plain.stderr)
                            self.assertTrue(listing.exists())
                            for path in exports:
                                if debug:
                                    self.assertEqual(path.read_bytes(), b'previous analysis')
                                else:
                                    self.assertFalse(path.exists())
                            if fmt == 'text' and index == 2:
                                self.assertNotRegex(listing.read_text(), r'(?m)^Shift\s+=')
                            if version == 2:
                                rows = (json.loads(listing.read_text())['records'] if fmt == 'json'
                                        else [json.loads(line) for line in listing.read_text().splitlines()])
                                for row in rows:
                                    self.assertFalse(row['binary_generated'])
                                    self.assertFalse(row['emits_bytes'])
                                    self.assertEqual(row['bytes_hex'], [])

    def test_invalid_shifts_with_individual_analysis_outputs(self):
        modes = (('--xref=xref.json', '--xref-data=true'),
                 ('--xref=xref.json', '--xref-instructions=true'),
                 ('--instruction-records-output=records.json', '--dependency-manifest=deps.json'),
                 ('--fceux-nl-ram-output=ram.nl',))
        for body in ('.byte (L2-L1)>>32', 'lda #((L2-L1)>>32)',
                     'lda L1+((L2-L1)>>32)', 'lda L1+((L2-L1)>>32),y'):
            source = '.org $8000\nL1: nop\nL2: ' + body + '\n'
            plain = self.run_tool('xasm', source)
            self.assertEqual(plain.returncode, 1, plain.stderr.decode())
            for arguments in modes:
                with self.subTest(body=body, arguments=arguments):
                    result = self.run_tool('xasm', source, arguments=arguments)
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    self.assertEqual(result.stderr, plain.stderr)
                    for filename in ('xref.json', 'records.json', 'ram.nl', 'deps.json'):
                        self.assertFalse((self.root / filename).exists())

    def test_unresolved_storage_with_rom_export_preserves_outputs(self):
        # Known limitation: an incomplete ROM layout cannot enumerate all NL
        # destinations, so collection fails before writing a diagnostic listing.
        cases = [(f'(L2-L1)>>(L2-L1-1+({count}))', b'invalid shift count in expression')
                 for count in (-1, 32, 63, 64)]
        cases += [('L1-L2', b'operand out of range')]
        for expression, error in cases:
            source = f'.org $8000\nL1: nop\nL2:\n.dsb {expression}\n.byte 1\n'
            for debug in (False, True):
                flags = ['--debug'] if debug else []
                listing = self.root / 'listing.txt'
                plain = self.run_tool('xasm', source, arguments=[*flags, f'--listing={listing}'])
                self.assertEqual(plain.returncode, 1, plain.stderr.decode())
                self.assertIn(error, plain.stderr)
                self.assertIn('SOURCE CODE', listing.read_text())
                for manifest in (False, True):
                    with self.subTest(expression=expression, debug=debug, manifest=manifest):
                        for filename in ('listing.txt', 'rom.0.nl', 'deps.json'):
                            (self.root / filename).write_bytes(b'previous analysis')
                        arguments = [*flags, f'--listing={listing}', '--fceux-nl-rom-prefix=rom.']
                        if manifest:
                            arguments += ['--dependency-manifest=deps.json']
                        result = self.run_tool('xasm', source, arguments=arguments)
                        self.assertEqual(result.returncode, 3 if manifest or expression == 'L1-L2' else 1,
                                         result.stderr.decode())
                        self.assertIn(b'could not collect assembly analysis', result.stderr)
                        for filename in ('listing.txt', 'rom.0.nl', 'deps.json'):
                            self.assertEqual((self.root / filename).read_bytes(), b'previous analysis')
        result = self.run_tool('xasm', '.org $8000\nL1: nop\nL2:\n.dsb L2-L1+1\n.byte 1\n',
                               arguments=('--fceux-nl-rom-prefix=control.',))
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual((self.root / 'output').read_bytes(), bytes.fromhex('EA 00 00 01'))
        self.assertIn('$8001#L2#', (self.root / 'control.0.nl').read_text())

    def test_unresolved_analysis_values_preserve_all_outputs(self):
        # The writer succeeds on this source. Only the test collector marks a
        # data value unresolved, so this reaches the pre-rename completeness gate.
        source = '.org $8000\nBase: nop\n.word Base+1\nlda $10\n'
        expected = bytes.fromhex('EA 01 80 A5 10')
        for debug in (False, True):
            for combined in (False, True):
                with self.subTest(debug=debug, combined=combined):
                    stem = f'incomplete-{debug}-{combined}'
                    suffixes = ('.xref', '.records', '.deps', '.ram.nl', '.rom.0.nl', '.listing.json')
                    arguments = [f'--xref={stem}.xref', '--xref-data=true']
                    if debug:
                        arguments += ['--debug']
                    if combined:
                        arguments += ['--xref-instructions=true', f'--instruction-records-output={stem}.records',
                                      f'--dependency-manifest={stem}.deps', f'--fceux-nl-ram-output={stem}.ram.nl',
                                      f'--fceux-nl-rom-prefix={stem}.rom.', f'--listing={stem}.listing.json',
                                      '--listing-format=json', '--listing-version=2']
                    # Positive control proves both the binary and data analysis
                    # normally succeed; a blanket collection failure cannot pass.
                    result = self.run_tool('xasm', source, arguments=arguments)
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    self.assertEqual(result.stderr, b'')
                    self.assertEqual((self.root / 'output').read_bytes(), expected)
                    facts = json.loads((self.root / (stem + '.xref')).read_text())
                    self.assertEqual([r['emitted_value'] for r in facts['data_directive_references']], [0x8001])
                    if combined:
                        self.assertTrue(all((self.root / (stem + suffix)).exists() for suffix in suffixes))
                    for suffix in suffixes:
                        (self.root / (stem + suffix)).write_bytes(b'previous analysis')
                    result = self.run_tool('xasm', source, {'XASM_TEST_UNRESOLVED_DATA': '1'},
                                           arguments=arguments)
                    self.assertEqual(result.returncode, 3, result.stderr.decode())
                    self.assertEqual(result.stderr, b'error: could not collect assembly analysis\n')
                    for suffix in suffixes:
                        self.assertEqual((self.root / (stem + suffix)).read_bytes(), b'previous analysis')

    def test_current_pc_data_analysis_uses_each_operand_address(self):
        mixed = ('Base: nop\n.byte $-Base,$-Base\n.word $-Base,$-Base\n'
                 '.dword $-Base,$-Base\n.char $-Base,$-Base\n.word Finish-$\nFinish: rts\n')
        cases = [
            ('Base: nop\n.word $-Base\n', bytes.fromhex('EA 01 00'), [(1, 2)]),
            (mixed, bytes.fromhex('EA 01 02 03 00 05 00 07 00 00 00 0B 00 00 00 0F 10 02 00 60'),
             [(1, 1), (2, 1), (3, 2), (5, 2), (7, 4), (11, 4), (15, 1), (16, 1), (17, 2)]),
            ('Base: nop\n.macro Emit\n.byte $-Base,$-Base\n.endm\n.rept 2\nEmit\n.endm\n',
             bytes.fromhex('EA 01 02 03 04'), [(1, 1), (2, 1), (3, 1), (4, 1)]),
            ('Base: nop\n.word Forward+($-Base),Forward+($-Base)\n'
             'Finish: rts\nForward .equ Finish-Base\n', bytes.fromhex('EA 06 00 08 00 60'), [(1, 2), (3, 2)]),
        ]
        for index, (body, expected, operands) in enumerate(cases):
            # The last origin crosses $FFFF; PC arithmetic must not wrap early.
            for origin in (0, 0x8000, 0xFFF8):
                for debug in (False, True):
                    source = f'.org {origin}\n' + body
                    flags = ['--debug'] if debug else []
                    plain = self.run_tool('xasm', source, arguments=flags)
                    self.assertEqual(plain.returncode, 0, plain.stderr.decode())
                    self.assertEqual((self.root / 'output').read_bytes(), expected)
                    for combined in (False, True):
                        with self.subTest(case=index, origin=origin, debug=debug, combined=combined):
                            stem = f'pc-{index}-{origin}-{debug}-{combined}'
                            arguments = flags + [f'--xref={stem}.xref', '--xref-data=true']
                            if combined:
                                arguments += ['--xref-instructions=true',
                                              f'--instruction-records-output={stem}.records',
                                              f'--listing={stem}.json', '--listing-format=json',
                                              '--listing-version=2', f'--dependency-manifest={stem}.deps',
                                              f'--fceux-nl-ram-output={stem}.ram.nl']
                                if origin == 0x8000:
                                    arguments += [f'--fceux-nl-rom-prefix={stem}.rom.']
                            result = self.run_tool('xasm', source, arguments=arguments)
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            self.assertEqual(result.stderr, plain.stderr)
                            self.assertEqual((self.root / 'output').read_bytes(), expected)
                            facts = json.loads((self.root / (stem + '.xref')).read_text())
                            refs = facts['data_directive_references']
                            self.assertEqual([(r['use_output_offset'], r['width_bytes']) for r in refs], operands)
                            for ref in refs:
                                offset, width = ref['use_output_offset'], ref['width_bytes']
                                self.assertEqual(ref['emitted_value'],
                                                 int.from_bytes(expected[offset:offset + width], 'little'))
                                self.assertEqual(int(ref['use_cpu_address'], 16), (origin + offset) & 0xFFFF)
                            if combined:
                                rows = json.loads((self.root / (stem + '.json')).read_text())['records']
                                self.assertEqual(b''.join(bytes.fromhex(''.join(r['bytes_hex'])) for r in rows),
                                                 expected)
                                records = json.loads((self.root / (stem + '.records')).read_text())['records']
                                for record in records:
                                    offset = record['output_offset']
                                    self.assertEqual(bytes(record['bytes']), expected[offset:offset + record['size']])

    def test_invalid_current_pc_data_expressions_preserve_analysis_outputs(self):
        for count in (-1, 32, 63, 64):
            for debug in (False, True):
                with self.subTest(count=count, debug=debug):
                    stem = f'pc-invalid-{count}-{debug}'
                    suffixes = ('.xref', '.records', '.deps', '.ram.nl', '.rom.0.nl')
                    for suffix in suffixes:
                        (self.root / (stem + suffix)).write_bytes(b'previous analysis')
                    arguments = [f'--xref={stem}.xref', '--xref-data=true', '--xref-instructions=true',
                                 f'--instruction-records-output={stem}.records',
                                 f'--dependency-manifest={stem}.deps', f'--fceux-nl-ram-output={stem}.ram.nl',
                                 f'--fceux-nl-rom-prefix={stem}.rom.', f'--listing={stem}.json',
                                 '--listing-format=json', '--listing-version=2']
                    if debug:
                        arguments += ['--debug']
                    result = self.run_tool('xasm', f'.org $8000\nBase: nop\n.word ($-Base)>>({count})\n',
                                           arguments=arguments)
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    self.assertIn(b'input.asm:3: error: invalid shift count in expression', result.stderr)
                    for suffix in suffixes:
                        self.assertEqual((self.root / (stem + suffix)).read_bytes(), b'previous analysis')
                    rows = json.loads((self.root / (stem + '.json')).read_text())['records']
                    self.assertTrue(rows)
                    self.assertTrue(all(not r['binary_generated'] and not r['emits_bytes']
                                        and not r['bytes_hex'] for r in rows))

    def test_invalid_right_shift_counts_preserve_output(self):
        for count in (-1, 32, 63, 64):
            for debug in (False, True):
                flags = ('--debug',) if debug else ()
                for form in ('literal', 'late_value', 'late_count', 'external'):
                    if form == 'external':
                        result = self.run_tool('xasm', f'.public Count\nCount .equ {count}\n',
                                               pure_binary=False, arguments=flags)
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        (self.root / 'output').rename(self.root / 'provider.o')
                        source = '.extrn Count:byte\n.dword (-2)>>Count\n'
                    elif form == 'literal':
                        source = f'.dword (-2)>>({count})\n'
                    elif form == 'late_value':
                        source = f'L1: nop\nL2: .dword (L2-L1+-3)>>({count})\n'
                    else:
                        source = f'L1: nop\nL2: .dword (-2)>>(L2-L1-1+({count}))\n'
                    for pure in ((False,) if form == 'external' else (False, True)):
                        with self.subTest(count=count, debug=debug, form=form, pure=pure):
                            result = self.run_tool('xasm', source, pure_binary=pure, arguments=flags)
                            if not pure and form != 'literal':
                                self.assertEqual(result.returncode, 0, result.stderr.decode())
                                (self.root / 'output').rename(self.root / 'consumer.o')
                                result = self.run_tool('xlnk',
                                    ('link{file=provider.o,origin=0}\n' if form == 'external' else '')
                                    + 'link{file=consumer.o,origin=0}\n')
                            self.assertEqual(result.returncode, 1, result.stderr.decode())
                            self.assertIn(b'invalid shift count in expression', result.stderr)

    def test_linker_instruction_signed_range_boundaries(self):
        for template, opcode, width, minimum in (
                ('lda #{}', 0xA9, 1, -128), ('lda [{},x]', 0xA1, 1, -128),
                ('lda [{}],y', 0xB1, 1, -128), ('lda.w {}', 0xAD, 2, -32768),
                ('jmp {}', 0x4C, 2, -32768), ('stx {},y', 0x96, 1, 0)):
            maximum = (1 << (8 * width)) - 1
            for value in (minimum - 1, minimum, -1, 0, maximum, maximum + 1):
                for debug in (False, True):
                    flags = ('--debug',) if debug else ()
                    for external in (False, True):
                        with self.subTest(template=template, value=value, debug=debug, external=external):
                            if external:
                                result = self.run_tool('xasm', f'.public Value\nValue .equ {value}\n',
                                                       pure_binary=False, arguments=flags)
                                self.assertEqual(result.returncode, 0, result.stderr.decode())
                                (self.root / 'output').rename(self.root / 'provider.o')
                                source, expr, expected = '.extrn Value:byte\n', 'Value', b''
                            else:
                                source, expr, expected = 'L1: nop\nL2:\n', f'(L2-L1-1+({value}))', b'\xea'
                            result = self.run_tool('xasm', source + template.format(expr) + '\n',
                                                   pure_binary=False, arguments=flags)
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            (self.root / 'output').rename(self.root / 'consumer.o')
                            result = self.run_tool('xlnk',
                                ('link{file=provider.o,origin=0}\n' if external else '')
                                + 'link{file=consumer.o,origin=0}\n')
                            if value < minimum or value > maximum:
                                self.assertEqual(result.returncode, 1, result.stderr.decode())
                                self.assertIn(f"instruction operand doesn't fit in {width} byte".encode(),
                                              result.stderr)
                            else:
                                self.assertEqual(result.returncode, 0, result.stderr.decode())
                                self.assertEqual(result.stderr, b'')
                                self.assertEqual((self.root / 'output').read_bytes(), expected + bytes([opcode])
                                                 + (value & maximum).to_bytes(width, 'little'))

    def test_linker_data_signed_range_warnings(self):
        for directive, minimum, maximum, width in (('byte', -128, 255, 1), ('word', -32768, 65535, 2)):
            for debug in (False, True):
                for external in (False, True):
                    with self.subTest(directive=directive, debug=debug, external=external):
                        values = (minimum - 1, minimum, -1, 0, maximum, maximum + 1)
                        names = ','.join(f'Value{i}' for i in range(len(values)))
                        flags = ('--debug',) if debug else ()
                        if external:
                            provider = '.public ' + names + '\n' + ''.join(
                                f'Value{i} .equ {value}\n' for i, value in enumerate(values))
                            result = self.run_tool('xasm', provider, pure_binary=False, arguments=flags)
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            (self.root / 'output').rename(self.root / 'provider.o')
                        source = (f'.extrn {names}:byte\n' if external else 'L1: nop\nL2:\n')
                        source += ''.join(f'.{directive} ' + (f'Value{i}' if external else
                                           f'L2-L1-1+({value})') + '\n' for i, value in enumerate(values))
                        result = self.run_tool('xasm', source, pure_binary=False, arguments=flags)
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        self.assertEqual(result.stderr, b'')
                        (self.root / 'output').rename(self.root / 'consumer.o')
                        result = self.run_tool('xlnk',
                            ('link{file=provider.o,origin=0}\n' if external else '')
                            + 'link{file=consumer.o,origin=0}\n')
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        self.assertEqual(result.stderr.count(b'warning:'), 2, result.stderr.decode())
                        self.assertEqual(result.stderr.count(b'out of range; truncated'), 2)
                        self.assertIn(f'operand {minimum - 1} out of range; truncated'.encode(), result.stderr)
                        self.assertIn(f'operand ${maximum + 1:X} out of range; truncated'.encode(), result.stderr)
                        self.assertEqual((self.root / 'output').read_bytes(), (b'' if external else b'\xea')
                                         + b''.join((value & maximum).to_bytes(width, 'little') for value in values))

    def test_linker_wide_truncation_warning_prints_full_value(self):
        # The evaluator uses long. Exercise the full diagnostic on hosts where
        # it can represent a result beyond 32 bits, without overflowing on ILP32.
        import ctypes
        if ctypes.sizeof(ctypes.c_long) < 8:
            self.skipTest('requires a linker with 64-bit long')
        source = 'L1: nop\nL2: .byte (L2-L1+$3FFFFFFF)*8-1\n.word (L2-L1+$3FFFFFFF)*8-1\n'
        result = self.run_tool('xasm', source, pure_binary=False)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        (self.root / 'output').rename(self.root / 'unit.o')
        result = self.run_tool('xlnk', 'link{file=unit.o,origin=0}\n')
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(result.stderr.count(b'$1FFFFFFFF out of range; truncated'), 2, result.stderr.decode())
        self.assertEqual((self.root / 'output').read_bytes(), bytes.fromhex('EA FF FF FF'))

    def test_signed_storage_comparisons_validate_the_final_count(self):
        for expression, count in (('1+2*((L2-L1+-2)<0)', 3),
                                  ('L2-L1+-2+2', 1), ('(L2-L1+-2)>0', 0)):
            for debug in (False, True):
                for pure in (False, True):
                    with self.subTest(expression=expression, debug=debug, pure=pure):
                        result = self.run_tool('xasm', 'L1: nop\nL2:\n.dsb ' + expression + '\n.byte $5A\n',
                                               pure_binary=pure, arguments=('--debug',) if debug else ())
                        if pure and not count:
                            self.assertEqual(result.returncode, 1, result.stderr.decode())
                            self.assertIn(b'operand out of range', result.stderr)
                            continue
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        if not pure:
                            (self.root / 'output').rename(self.root / 'unit.o')
                            result = self.run_tool('xlnk', 'link{file=unit.o,origin=0}\n')
                            if not count:
                                self.assertEqual(result.returncode, 1, result.stderr.decode())
                                self.assertIn(b'storage size out of range', result.stderr)
                                continue
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                        self.assertEqual((self.root / 'output').read_bytes(), b'\xea' + b'\0'*count + b'\x5a')

    def test_signed_omitted_array_counts_allow_zero_but_reject_negative(self):
        types = ('.struc Inner\nv .byte\nw .byte\n.ends\n'
                 '.struc S\nhd .byte\nitems .Inner[N]\ntl .byte\n.ends\n')
        for count in (-1, 0, 1):
            source = (types + 'N .equ L2-L1+-1\nL1:\n' + 'nop\n'*(count+1)
                      + 'L2:\n.S {$11}\nAfter: .word After\n')
            for origin in (0, 0x8000):
                for debug in (False, True):
                    for pure in (False, True):
                        with self.subTest(count=count, origin=origin, debug=debug, pure=pure):
                            result = self.run_tool('xasm', (f'.org {origin}\n' if pure else '') + source,
                                                   pure_binary=pure, arguments=('--debug',) if debug else ())
                            if pure and count < 0:
                                self.assertEqual(result.returncode, 1, result.stderr.decode())
                                self.assertIn(b'operand out of range', result.stderr)
                                continue
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            if not pure:
                                (self.root / 'output').rename(self.root / 'unit.o')
                                result = self.run_tool('xlnk', f'link{{file=unit.o,origin={origin}}}\n')
                                if count < 0:
                                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                                    self.assertIn(b'storage size out of range', result.stderr)
                                    continue
                                self.assertEqual(result.returncode, 0, result.stderr.decode())
                            expected = b'\xea'*(count+1) + b'\x11' + b'\0'*(2*count+1)
                            expected += (origin+len(expected)).to_bytes(2, 'little')
                            self.assertEqual((self.root / 'output').read_bytes(), expected)

    def test_storage_counts_are_bounded_before_any_output(self):
        expressions = [b'\x04' + n.to_bytes(4, 'big') for n in
                       (0, 65536, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF)]
        expressions.append(bytes.fromhex('240101'))  # -1
        for segment in ('code', 'data'):
            for expression in expressions:
                with self.subTest(segment=segment, expression=expression):
                    data = object_file(**{segment: bytes.fromhex('fd0000f3')}, expressions=[expression])
                    (self.root / 'unit.o').write_bytes(data)
                    (self.root / 'second').write_bytes(b'previous second')
                    result = self.run_tool('xlnk', 'pad{size=1}\noutput{file=second}\n'
                                           'link{file=unit.o,origin=0}\n', limited=True)
                    self.assertNotEqual(result.returncode, 0, result.stderr.decode())
                    self.assertIn(b'storage size', result.stderr)
                    self.assertEqual((self.root / 'second').read_bytes(), b'previous second')

    def test_valid_storage_count_boundaries(self):
        for count in (1, 65535):
            with self.subTest(count=count):
                data = object_file(code=bytes.fromhex('fd0000f3'),
                                   expressions=[b'\x04' + count.to_bytes(4, 'big')])
                (self.root / 'unit.o').write_bytes(data)
                result = self.run_tool('xlnk', 'link{file=unit.o,origin=0}\n', limited=True)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual((self.root / 'output').read_bytes(), b'\0' * count)

    def test_forward_storage_counts_are_rejected_before_any_output(self):
        # DSB target+5; target: The first layout pass cannot
        # determine the storage size without using a yet-unresolved address.
        for segment in ('code', 'data'):
            data = object_file(**{segment: bytes.fromhex('fd0000f600f3')},
                               expressions=[bytes.fromhex('100800000105')])
            (self.root / 'unit.o').write_bytes(data)
            (self.root / 'second').write_bytes(b'previous second')
            result = self.run_tool('xlnk', 'pad{size=1}\noutput{file=second}\n'
                                   'link{file=unit.o,origin=0}\n')
            self.assertNotEqual(result.returncode, 0, result.stderr.decode())
            self.assertIn(b'storage size', result.stderr)
            self.assertEqual((self.root / 'second').read_bytes(), b'previous second')

    def test_storage_counts_using_resolved_labels_remain_valid(self):
        # target: DSB target+5; DW target, linked at address 1.
        data = object_file(code=bytes.fromhex('f600fd0000f90001f3'),
                           expressions=[bytes.fromhex('100800000105'), bytes.fromhex('080000')])
        (self.root / 'unit.o').write_bytes(data)
        result = self.run_tool('xlnk', 'link{file=unit.o,origin=1}\n')
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual((self.root / 'output').read_bytes(), b'\0' * 6 + b'\x01\0')

    def test_immediate_storage_count_boundaries(self):
        for segment in ('code', 'data'):
            (self.root / 'unit.o').write_bytes(object_file(**{segment: bytes.fromhex('fcfffff3')}))
            (self.root / 'second').write_bytes(b'previous second')
            result = self.run_tool('xlnk', 'pad{size=1}\noutput{file=second}\n'
                                   'link{file=unit.o,origin=0}\n', limited=True)
            self.assertNotEqual(result.returncode, 0, result.stderr.decode())
            self.assertIn(b'storage size', result.stderr)
            self.assertEqual((self.root / 'second').read_bytes(), b'previous second')
        for command, count in (('fb00', 1), ('fbff', 256), ('fc0000', 1), ('fcfffe', 65535)):
            (self.root / 'unit.o').write_bytes(object_file(code=bytes.fromhex(command + 'f3')))
            result = self.run_tool('xlnk', 'link{file=unit.o,origin=0}\n', limited=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertEqual((self.root / 'output').read_bytes(), b'\0' * count)

    def test_empty_command_line_string_definitions(self):
        for argument in ('-DValue=""', '-DValue='):
            result = self.run_tool('xasm', '.DB "[",Value,"]"\n', arguments=[argument])
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertEqual((self.root / 'output').read_bytes(), b'[]')

    def test_command_line_definition_allocation_failures(self):
        for argument in ('-DValue="text"', '-DValue=""', '-DValue=', '-DValue=1', '-DValue'):
            faults = ['XASM_TEST_VALUE_NODE_FAIL']
            if argument not in ('-DValue=1', '-DValue'):
                faults.append('XASM_TEST_STRING_TEXT_FAIL')
            for fault in faults:
                with self.subTest(argument=argument, fault=fault):
                    result = self.run_tool('xasm', '.DB Value\n',
                                           {fault: '1', 'XASM_TEST_EMPTY_STACK_AT_EXIT': '1'},
                                           arguments=[argument])
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    self.assertIn(b'INJECT_', result.stderr)
                    self.assertIn(b'out of memory creating command-line definition', result.stderr)

    def test_missing_source_releases_root_scope(self):
        environment = dict(os.environ, XASM_TEST_EMPTY_STACK_AT_EXIT='1')
        result = subprocess.run([str(self.tools['xasm']), 'missing.asm', '-DValue="text"'],
                                cwd=self.root, env=environment, capture_output=True, timeout=20)
        self.assertEqual(result.returncode, 1, result.stderr.decode())
        self.assertIn(b'could not open', result.stderr)
        self.assertNotRegex(result.stderr, SANITIZER_REPORT)

    def test_linker_hash_allocation_failures_preserve_all_outputs(self):
        # One exported constant, one exported label and the unit registration
        # exercise every hash insertion path, after all six table allocations.
        # Use a second object to add an exported label without altering metadata.
        (self.root / 'unit.o').write_bytes(self.object_with_metadata())
        (self.root / 'label.o').write_bytes(object_file(code=bytes.fromhex('f601004cf400eaf3')))
        for index in range(20):
            (self.root / 'second').write_bytes(b'previous second')
            result = self.run_tool('xlnk', 'pad{size=1}\noutput{file=second}\n'
                                   'link{file=unit.o,origin=$8000}\nlink{file=label.o}\n',
                                   {'XLNK_TEST_HASH_FAIL': str(index)})
            if b'INJECT_HASH' not in result.stderr:
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual(index, 10)
                break
            self.assertEqual(result.returncode, 1, result.stderr.decode())
            self.assertIn(b'out of memory', result.stderr)
            self.assertEqual((self.root / 'second').read_bytes(), b'previous second')
        else:
            self.fail('hash allocation sweep did not reach the end')

    def test_rejected_command_line_definitions_release_their_values(self):
        cases = [(['-DValue=1', '-DValue=2'], '.DB Value\n', b'\x01', b'already defined'),
                 (['-DValue=1', '-DValue'], '.DB Value\n', b'\x01', b'already defined'),
                 (['-DValue="first"', '-DValue="second"'], '.DB Value\n', b'first', b'already defined'),
                 (['-Dbad-name=3'], '.DB 0\n', b'\0', b'not a valid identifier'),
                 (['-Dbad-name="unused"'], '.DB 0\n', b'\0', b'not a valid identifier')]
        for arguments, source, expected, diagnostic in cases:
            with self.subTest(arguments=arguments):
                result = self.run_tool('xasm', source, arguments=arguments)
                # Existing CLI behavior ignores rejected definitions and keeps
                # the first value. LeakSanitizer checks ownership on these paths.
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertIn(diagnostic, result.stderr)
                self.assertEqual((self.root / 'output').read_bytes(), expected)

    @staticmethod
    def object_with_metadata():
        # Exported constant, external referring to it, and a string expression
        # exercise all variable-length metadata arrays and owned strings.
        return (bytes.fromhex('face140001') + b'\0K\x01\x01' + b'\0'
                 + bytes.fromhex('0001') + b'\0\0K'
                 + bytes.fromhex('000001f3000004f80000f30001') + b'\x05\0X')

    def test_object_allocation_failures_are_clean(self):
        (self.root / 'unit.o').write_bytes(self.object_with_metadata())
        for index in range(100):
            result = self.run_tool('xlnk', 'link{file=unit.o,origin=$8000}\n',
                                   {'XLNK_TEST_UNIT_FAIL': str(index)})
            if b'INJECT_UNIT' not in result.stderr:
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertGreater(index, 5)
                break
            self.assertNotEqual(result.returncode, 0, result.stderr.decode())
        else:
            self.fail('object allocation sweep did not reach the end')


if __name__ == '__main__':
    unittest.main()
