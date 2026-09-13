#!/usr/bin/env python3
"""Run malformed inputs and allocation failures through the actual tools."""
import os
from pathlib import Path
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
        build_tool('xlnk', cls.tools['xlnk'], 'TEST_INPUT_CFLAGS', {'unit.c': 'tests/test_unit_alloc.c'})

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='xorcyst-input-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def run_tool(self, name, content, environment=None):
        source = 'input.asm' if name == 'xasm' else 'input.script'
        (self.root / source).write_bytes(content.encode() if isinstance(content, str) else content)
        (self.root / 'output').write_bytes(b'previous output')
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(('XASM_TEST_', 'XLNK_TEST_'))}
        env.update(environment or {})
        result = subprocess.run([str(self.tools[name]), source, '-o', 'output',
                                 *(['--pure-binary'] if name == 'xasm' else [])],
                                cwd=self.root, env=env, capture_output=True, timeout=20)
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
