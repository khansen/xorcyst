#!/usr/bin/env python3
"""Resolve forward constants before validating and encoding their uses."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

XASM = Path(sys.argv.pop(1)).resolve()
XLNK = Path(os.environ.get('XLNK', XASM.with_name('xlnk'))).resolve()


class Expressions(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='xasm-expressions-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def assemble(self, source, pure, debug=False, listing=False):
        (self.root / 'input.asm').write_text(source)
        output = self.root / 'output'
        output.write_bytes(b'previous output')
        flags = ['--pure-binary'] if pure else []
        if debug: flags += ['--debug']
        if listing: flags += ['--listing=listing.json', '--listing-format=json', '--xref=xref.json']
        result = subprocess.run([str(XASM), *flags, 'input.asm', '-o', output.name],
                                cwd=self.root, capture_output=True, timeout=10)
        return result, output

    def linked(self, output):
        (self.root / 'link.script').write_text('link{file=output,origin=$8000}\n')
        result = subprocess.run([str(XLNK), 'link.script', '-o', 'linked.bin'],
                                cwd=self.root, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        return (self.root / 'linked.bin').read_bytes()

    def assert_output(self, source, expected):
        for pure in (False, True):
            for debug in (False, True):
                for listing in (False, True):
                    with self.subTest(pure=pure, debug=debug, listing=listing):
                        result, output = self.assemble(('.org $8000\n' if pure else '') + source,
                                                      pure, debug, listing)
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        data = output.read_bytes() if pure else self.linked(output)
                        self.assertEqual(data, expected)

    def test_forward_aliases_in_data_and_instruction_operands(self):
        self.assert_output('.byte Value\nlda #Value\nlda Value\n.word Value+1\n'
                           'Value .equ Alias\nAlias .equ Final\nFinal=2\n',
                           bytes.fromhex('02 A9 02 A5 02 03 00'))

    def test_aliases_to_labels_remain_local_without_analysis_output(self):
        for depth in range(1, 9):
            with self.subTest(depth=depth):
                definitions = ''.join(f'Ref{i} .equ Ref{i+1}\n' for i in range(depth))
                self.assert_output('jmp Ref0\n.word Ref0\n' + definitions
                                   + f'Ref{depth} .equ Target\nTarget: nop\n',
                                   bytes.fromhex('4C 05 80 05 80 EA'))

    def test_aliases_capture_values_at_each_use(self):
        self.assert_output('Value=2\nAlias .equ Value\n.byte Alias\nValue=3\n.byte Alias,Value\n',
                           bytes.fromhex('02 02 03'))
        self.assert_output('.byte Alias\nAlias .equ Value\nValue=3\n.byte Alias\nValue=4\n.byte Alias\n',
                           bytes.fromhex('04 03 04'))

    def test_nested_indexed_instance_members_in_objects(self):
        source = ('.struc Point\npx .byte\npy .byte\n.ends\n'
                  '.struc Box\npoints .Point[2]\n.ends\nv .Box\n'
                  'Alias .equ v.points[1].py\n.word Alias\n')
        for debug in (False, True):
            for listing in (False, True):
                with self.subTest(debug=debug, listing=listing):
                    result, output = self.assemble(source, False, debug, listing)
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    self.assertEqual(self.linked(output), bytes.fromhex('00 00 00 00 03 80'))

    def test_alias_to_untyped_label_member_fails_cleanly(self):
        source = ('.struc Point\npx .byte\npy .byte\n.ends\n'
                  'v: .Point {1,2}\nAlias .equ v.py\nlda Alias\n')
        for pure in (False, True):
            for debug in (False, True):
                for listing in (False, True):
                    with self.subTest(pure=pure, debug=debug, listing=listing):
                        result, output = self.assemble(source, pure, debug, listing)
                        self.assertEqual(result.returncode, 1, result.stderr.decode())
                        self.assertIn(b"`v' is not a structure", result.stderr)
                        self.assertEqual(output.read_bytes(), b'previous output')

    def test_aliases_in_macro_rept_and_unary_expressions(self):
        self.assert_output('.macro Emit\n.byte <Value,>Value,-Neg\n.endm\n'
                           '.rept 2\nEmit\n.endm\n'
                           'Value .equ Alias\nAlias=$1234\nNeg .equ Other\nOther=-2\n',
                           bytes.fromhex('34 12 02 34 12 02'))

    def test_sizeof_label_with_multiple_forward_aliases(self):
        self.assert_output('.struc Point\npx .byte\npy .byte\n.ends\n'
                           '.struc Array\npoints .Point[Count]\ntail .byte\n.ends\n'
                           'Payload: .Array {{1,2},$5A}\n'
                           'Count .equ Alias\nAlias .equ Final\nFinal=2\n'
                           'End: .byte sizeof(Payload)\n', bytes.fromhex('01 02 00 00 5A 05'))

    def test_forward_alias_to_external_symbol_links(self):
        result, output = self.assemble(".word Value\nValue .equ Alias\nAlias .equ RemoteLabel\n", False)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"RemoteLabel' undeclared", result.stderr)
        (self.root / 'provider.asm').write_text('.public RemoteLabel\nRemoteLabel: .byte $AA\n')
        provider = subprocess.run([str(XASM), 'provider.asm', '-o', 'provider.o'],
                                  cwd=self.root, capture_output=True)
        self.assertEqual(provider.returncode, 0, provider.stderr.decode())
        (self.root / 'link.script').write_text('link{file=output,origin=$8000}\n'
                                               'link{file=provider.o,origin=$8002}\n')
        linked = subprocess.run([str(XLNK), 'link.script', '-o', 'linked.bin'],
                                cwd=self.root, capture_output=True)
        self.assertEqual(linked.returncode, 0, linked.stderr.decode())
        self.assertEqual((self.root / 'linked.bin').read_bytes(), bytes.fromhex('02 80 AA'))

    def test_cycles_fail_once_per_use_and_preserve_output(self):
        for definitions in ('Value .equ Value\n', 'Value .equ Alias\nAlias .equ Value\n',
                            'Value .equ Alias+1\nAlias .equ <Value\n'):
            for pure in (False, True):
                with self.subTest(definitions=definitions, pure=pure):
                    result, output = self.assemble('.byte Value\n' + definitions, pure)
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    self.assertEqual(result.stderr.count(b'circular constant definition'), 1,
                                     result.stderr.decode())
                    self.assertEqual(result.stderr.count(b'error:'), 1, result.stderr.decode())
                    self.assertEqual(output.read_bytes(), b'previous output')


if __name__ == '__main__':
    unittest.main()
