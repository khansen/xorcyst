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

    def test_explicit_sizeof_captures_known_reassignable_counts(self):
        self.assert_output('Count=1\n.struc S\nhead .byte\nitems .word[Count]\ntail .byte\n.ends\n'
                           '.byte sizeof(S)\nCount=3\n.byte sizeof(S)\n', bytes.fromhex('04 08'))

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

    def test_indexed_structure_sizes_resolve_named_counts(self):
        for definition, placement in (("", "literal"), ("Count=2\n", "before"),
                                      ("Count .equ 2\n", "before"),
                                      (".define Count 2\n", "before"),
                                      ("Count .equ Other\nOther=2\n", "after"),
                                      ("Count .equ -Other\nOther=-2\n", "after")):
            types = ('.struc Point\npx .byte\npy .byte\n.ends\n'
                     '.struc S\nhead .byte\npoints .Point[Count]\ntail .byte\n.ends\n'
                     '.struc Box\nprefix .byte\narr .S[2]\nsuffix .byte\n.ends\n')
            operands = ('lda inst[1].head\n'
                        '.word inst[1]-inst,inst[1].points[1].py-inst,inst[1].tail-inst\n'
                        '.word box.arr[1].head-box,box.arr[1].points[1].py-box\n'
                        '.byte sizeof(S),sizeof(Box)\n.word inst,box\n')
            if placement == 'literal': types = types.replace('[Count]', '[2]')
            for external in (False, True):
                for debug in (False, True):
                    for listing in (False, True):
                        with self.subTest(definition=definition, placement=placement,
                                          external=external, debug=debug, listing=listing):
                            declarations = ((definition if placement == 'before' else '') + types)
                            storage = '.dataseg\ninst .S[4]\nbox .Box\n.codeseg\n'
                            source = declarations + ('.extrn inst:S\n.extrn box:Box\n' if external else storage)
                            source += operands + (definition if placement == 'after' else '')
                            result, output = self.assemble(source, False, debug, listing)
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            script = 'ram{start=$0200,end=$07FF}\nlink{file=output,origin=$8000}\n'
                            if external:
                                (self.root / 'provider.asm').write_text(
                                    declarations + '.public inst,box\n' + storage
                                    + (definition if placement == 'after' else ''))
                                provider = subprocess.run([str(XASM), 'provider.asm', '-o', 'provider.o'],
                                                          cwd=self.root, capture_output=True, timeout=10)
                                self.assertEqual(provider.returncode, 0, provider.stderr.decode())
                                script += 'link{file=provider.o}\n'
                            (self.root / 'link.script').write_text(script)
                            binary = self.root / 'linked.bin'
                            binary.unlink(missing_ok=True)
                            linked = subprocess.run([str(XLNK), 'link.script', '-o', binary.name],
                                                    cwd=self.root, capture_output=True, timeout=10)
                            self.assertEqual(linked.returncode, 0, linked.stderr.decode())
                            data = binary.read_bytes()
                            inst = int.from_bytes(data[-4:-2], 'little')
                            self.assertGreaterEqual(inst, 0x200)
                            expected = (b'\xad' + (inst + 6).to_bytes(2, 'little')
                                        + bytes.fromhex('06 00 0A 00 0B 00 07 00 0B 00 06 0E'))
                            self.assertEqual(data[:-4], expected)

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

    def test_repeated_alias_cycle_reports_one_error_per_use(self):
        depth = 12
        definitions = ''.join(f'A{i} .equ A{i+1}+A{i+1}\n' for i in range(depth))
        definitions += f'A{depth} .equ A{depth}\n'
        for field in (False, True):
            body = ('.struc S\nitems .byte[Count]\n.ends\n.S {{1}}\n' if field
                    else '.byte Count\n')
            for pure in (False, True):
                with self.subTest(field=field, pure=pure):
                    result, output = self.assemble(body + 'Count .equ A0\n' + definitions, pure)
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    self.assertEqual(result.stderr.count(b'error:'), 1, result.stderr.decode())
                    self.assertIn(b'circular', result.stderr)
                    self.assertEqual(output.read_bytes(), b'previous output')

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
