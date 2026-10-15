#!/usr/bin/env python3
"""Run malformed inputs and allocation failures through the actual tools."""
import os
from pathlib import Path
import resource
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
                   {'symtab.c': 'tests/test_symtab_alloc.c', 'astnode.c': 'tests/test_astnode_alloc.c'})
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

    def test_linker_keeps_negative_immediates_and_backward_branches(self):
        # An expression-backed immediate exercises write_instr, rather than a
        # byte already folded by xasm. The branch still uses a signed offset.
        code = bytes.fromhex('f7a90000f7d00001f3')
        expressions = [bytes.fromhex('240101'), bytes.fromhex('0100')]
        (self.root / 'unit.o').write_bytes(object_file(code=code, expressions=expressions))
        result = self.run_tool('xlnk', 'link{file=unit.o,origin=0}\n')
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual((self.root / 'output').read_bytes(), bytes.fromhex('A9 FF D0 FC'))

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
