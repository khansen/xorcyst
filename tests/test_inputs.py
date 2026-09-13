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
        build_tool('xasm', cls.tools['xasm'], 'TEST_INPUT_CFLAGS', {'symtab.c': 'tests/test_symtab_alloc.c'})
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

    def test_malformed_source_is_rejected_without_crashes_or_leaks(self):
        cases = ('LDA #(', 'PROC Incomplete\nNOP\n', 'MACRO Incomplete\nNOP\n',
                 'IF 1\nLDA #1\n', '.DB 1,\n', 'Value .EQU 1\nLDA Value::Missing\n',
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

    def test_symbol_allocation_failures_reach_the_cli_without_partial_outputs(self):
        source = ('STRUC Pair\nFirst BYTE\nSecond BYTE\nENDS\n'
                  'UNION Either\nByteValue BYTE\nWordValue WORD\nENDS\n'
                  'ENUM Values\nOne\nTwo\nENDE\nRECORD Bits first:1,rest:7\n'
                  'Port .EQU $10\nMACRO Load\nLDA Port\nENDM\n'
                  'DATASEG\nBuffer BYTE\nCODESEG\n.ORG $8000\nEntry:\nLoad\nRTS\nEND\n')
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
