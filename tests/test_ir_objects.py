#!/usr/bin/env python3
"""Independent wire oracle and source/object/link integration controls."""
import copy
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile
import unittest

from test_ir_frontend import build as build_assembler

REPO = Path(__file__).resolve().parents[1]
WIDTHS = (2, 3, 5, 6, 6, 9, 5, 6, 7, 5)


def encode(tables, blobs):
    """Independent fixed-width encoder, deliberately without validation."""
    tables = copy.deepcopy(tables)
    at = 92 + len(blobs) * 8 + sum(len(tables[i]) * WIDTHS[i] * 4 for i in range(1, 10))
    payload = bytearray()
    tables[0] = []
    for blob in blobs:
        tables[0].append((at, len(blob)))
        padded = blob + bytes((-len(blob)) % 4)
        payload += padded
        at += len(padded)
    header = bytearray(b'XOROBJ2\n' + struct.pack('<I', at))
    body = bytearray()
    at = 92
    for width, table in zip(WIDTHS, tables):
        header += struct.pack('<II', at, len(table))
        for row in table:
            assert len(row) == width
            body += struct.pack('<' + 'I' * width, *row)
        at += len(table) * width * 4
    return bytes(header + body + payload)


def fixture():
    t = [[] for _ in WIDTHS]
    blobs = [b'input.asm', b'Start', b'\xea']
    t[1] = [(0, 0, 0)]
    t[2] = [(0, 1, 1, 1, 17)]
    t[3] = [(1, 2, 1, 0, 1, 0)]
    t[6] = [(1, 0, 0, 1, 0)]
    t[8] = [(1, 2, 0, 0, 0, 1, 0)]
    return t, blobs


def data_object(expressions, fixups=None, blobs=None):
    t = [[] for _ in WIDTHS]
    t[4] = expressions
    t[7] = fixups if fixups is not None else [(len(expressions)-1, 1, 0, 0, 0, 0)]
    t[8] = [(2, 0, len(t[7]), 0, 0, 0, 0)]
    return encode(t, blobs or [])


class Objects(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        d = tempfile.TemporaryDirectory(prefix='xorcyst-i4-tools-')
        cls.addClassCleanup(d.cleanup)
        cls.tools = Path(d.name)
        cls.assembler = cls.tools/'xasm'
        cls.linker = cls.tools/'xlnk'
        cls.codec = cls.tools/'codec'
        cls.failures = cls.tools/'failures'
        cls.heap = cls.tools/'heap'
        cls.faultlinker = cls.tools/'faultlinker'
        build_assembler(cls.assembler)
        cc = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-I', str(REPO), '-Wall', '-Wextra',
              '-Wno-unused-parameter', '-Wno-sign-compare', *shlex.split(os.environ.get('TEST_IR_CFLAGS', '-O2 -g -Werror'))]
        for output, sources in ((cls.linker, ['xlnk.c', 'ir/link.c', 'ir/object.c', 'tests/test_ir_heap_alloc.c', 'ir/value.c',
                                 'ir/layout.c', 'opcode.c', 'script.c', 'output_file.c', 'my_getopt.c']),
                                (cls.faultlinker, ['xlnk.c', 'ir/link.c', 'ir/object.c', 'tests/test_ir_heap_alloc.c', 'ir/value.c',
                                 'ir/layout.c', 'opcode.c', 'script.c', 'tests/test_ir_link_io.c', 'my_getopt.c']),
                                (cls.codec, ['ir/heap.c', 'tests/test_ir_object_codec.c', 'opcode.c']),
                                (cls.heap, ['tests/test_ir_heap.c', 'tests/test_ir_heap_alloc.c']),
                                (cls.failures, ['tests/test_ir_heap_alloc.c', 'tests/test_ir_object_failures.c','tests/test_ir_alloc.c',
                                 'tests/test_ir_layout_alloc.c','tests/test_ir_object_alloc.c',
                                 'tests/test_ir_program_alloc.c','tests/test_ir_link_alloc.c','opcode.c'])):
            r = subprocess.run([*cc, *[str(REPO/s) for s in sources], '-o', str(output)], capture_output=True, text=True)
            if r.returncode: raise AssertionError(r.stderr)

    def test_link_heap_reuses_storage_and_handles_every_mapping_failure(self):
        r = subprocess.run([str(self.heap)], capture_output=True, text=True, timeout=30)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(r.stderr, '')
        self.assertIn('mapping failures preserved ownership', r.stdout)

    def setUp(self):
        d = tempfile.TemporaryDirectory(prefix='xorcyst-i4-case-')
        self.addCleanup(d.cleanup)
        self.directory = Path(d.name)

    def run_tool(self, tool, args, success=True, **options):
        r = subprocess.run([str(tool), *args], cwd=self.directory, capture_output=True, text=True, timeout=30, **options)
        self.assertGreaterEqual(r.returncode, 0, r.stderr)
        self.assertNotRegex(r.stderr, r'AddressSanitizer|LeakSanitizer|runtime error:')
        if success: self.assertEqual(r.returncode, 0, r.stderr)
        else: self.assertNotEqual(r.returncode, 0, r.stderr)
        return r

    def assemble(self, sources):
        for i, source in enumerate(sources):
            (self.directory/f'{i}.asm').write_text(source)
            self.run_tool(self.assembler, [f'{i}.asm', '-o', f'{i}.o'])

    def link(self, script=None, expected=None, error=None):
        if script is None:
            script = 'output{file=out}\n' + ''.join(f'link{{file={p.name}}}\n' for p in sorted(self.directory.glob('*.o')))
        (self.directory/'link.s').write_text(script)
        (self.directory/'out').write_bytes(b'previous output')
        r = self.run_tool(self.linker, ['link.s'], success=error is None)
        if error:
            self.assertIn(error, r.stderr)
            self.assertEqual((self.directory/'out').read_bytes(), b'previous output')
        elif expected is not None:
            self.assertEqual((self.directory/'out').read_bytes(), bytes.fromhex(expected))
        self.assertFalse(list(self.directory.glob('*.tmp*')))
        return r

    def codec_check(self, data, valid=True):
        (self.directory/'object').write_bytes(data)
        (self.directory/'encoded').write_bytes(b'previous output')
        self.run_tool(self.codec, ['object', 'encoded'], success=valid)
        self.assertEqual((self.directory/'encoded').read_bytes(), data if valid else b'previous output')

    def test_decoded_tables_and_payloads_can_be_mutated_safely(self):
        tables, blobs = fixture()
        (self.directory/'object').write_bytes(encode(tables, blobs))
        self.run_tool(self.codec, ['object', 'encoded', 'mutate'])
        tables[1].append(tables[1][0])
        tables[2][0] = (0, 1, 1, 1, 18)
        blobs[2] += b'\xea'
        self.assertEqual((self.directory/'encoded').read_bytes(), encode(tables, blobs))
        original_tables, original_blobs = fixture()
        self.run_tool(self.codec, ['object', 'encoded', 'add-blob'])
        self.assertEqual((self.directory/'encoded').read_bytes(),
                         encode(original_tables, original_blobs + [original_blobs[0]]))
        (self.directory/'encoded').write_bytes(b'previous output')
        self.run_tool(self.codec, ['object', 'encoded', 'corrupt'], success=False)
        self.assertEqual((self.directory/'encoded').read_bytes(), b'previous output')

    def test_fixed_code_runs_preserve_labels_ranges_and_branch_fixups(self):
        self.assemble(['.public Start, TailFixed\nStart: nop\nlda $10\nMiddle: lda.w $10\n'
                       'lda #$ff\nbne TailFixed\n.db 7\nTailFixed: rts\n.dw Start,Middle,TailFixed\n'])
        self.link('output{file=out}\nlink{file=0.o,origin=$8000}\n',
                  expected='EA A5 10 AD 10 00 A9 FF D0 01 07 60 00 80 03 80 0B 80')
        self.assemble(['lda #$1ff\nlda.w -1\nlda [$ff],y\n'])
        self.link(expected='A9 FF AD FF FF B1 FF')
        self.assemble(['bne Target\n.dsb 128\nTarget: nop\n'])
        self.link(error='operand out of range')

    def test_coalesced_raw_runs_keep_shared_and_interior_anchors(self):
        self.assemble(['.public AddrA,AddrB,AddrC,AddrD\nAddrA:\nAddrB:\n.db 17\nAddrC:\n.db 34\nAddrD:\n.dw AddrA,AddrB,AddrC,AddrD\n'])
        self.link('output{file=out}\nlink{file=0.o,origin=$8000}\n',
                  expected='11 22 00 80 00 80 01 80 02 80')
        self.assemble(['.public Head,Tail\n.dsb Tail-Head\nHead: .db 1,2\nTail: rts\n.dw Head,Tail\n'])
        self.link(expected='00 00 01 02 60 02 00 04 00')

    def test_writer_shares_owner_located_leaves_and_retains_use_spans(self):
        self.assemble(['.extrn Ext:word\n.public Start\nStart:nop\n'
                       '.dw Ext,Start,Ext,Start\n.dw Ext+7\n.db ^Ext\n',
                       '.public Ext\nExt:rts\n'])
        data=(self.directory/'0.o').read_bytes()
        def table(index):
            offset,count=struct.unpack_from('<II',data,12+8*index)
            return [struct.unpack_from('<'+'I'*WIDTHS[index],data,offset+i*4*WIDTHS[index])
                    for i in range(count)]
        expressions=table(4)
        leaves=[r for r in expressions if r[0] in (3,4)]
        self.assertTrue(any(r[0]==3 for r in leaves))
        self.assertTrue(any(r[0]==4 for r in leaves))
        self.assertEqual(len(leaves),len({(r[0],r[1]) for r in leaves}))
        self.assertTrue(all(r[4:]==(0,0) for r in leaves))
        for tag in (1,5,32):
            records=[r for r in expressions if r[0]==tag]
            self.assertTrue(records,tag)
            self.assertTrue(all(r[4]==1 for r in records),tag)
        self.assertTrue(all(r[4]==1 for r in table(7)))
        self.link(expected='EA 0C 00 00 00 0C 00 00 00 13 00 FF 60')

    def test_spanless_reference_cycles_keep_owner_diagnostics(self):
        t=[[] for _ in WIDTHS]
        t[1]=[(0,0,0)]
        t[2]=[(0,7,1,7,8),(0,9,1,9,8),(0,11,1,11,8)]
        t[3]=[(1,1,0,0,1,0),(2,1,0,1,1,1)]
        t[4]=[(3,1,0,0,0,0),(3,0,0,0,0,0)]
        t[7]=[(0,1,0,0,1,2)]
        t[8]=[(2,0,1,0,0,1,2)]
        (self.directory/'0.o').write_bytes(encode(t,[b'input.asm',b'A',b'B']))
        r=self.link(error='circular')
        self.assertRegex(r.stderr,r'input.asm:(7|9): error: circular')
        self.assertNotIn('location unavailable',r.stderr)

    def test_allocation_failures_release_every_owned_object(self):
        r = self.run_tool(self.failures, [])
        self.assertEqual(len(r.stdout.splitlines()), 6)
        self.assertTrue(all('allocation failures preserved ownership' in line for line in r.stdout.splitlines()[:5]))
        self.assertIn('pooled-link mapping failures preserved ownership', r.stdout.splitlines()[5])

    def test_imported_initializer_lengths_follow_provider_kind(self):
        for value, expected in [('7','07 00 00 00 09'), ('"AB"','41 42 00 00 09'), ('"ABCD"','41 42 43 44 09'), ('""','00 00 00 00 09')]:
            for directive in ('.byte','.char'):
                with self.subTest(value=value,directive=directive):
                    self.assemble([f'.extrn Val:byte\n.struc S\nf {directive}[4]\ng .byte\n.ends\nv .S {{Val,9}}\n',
                                   f'.public Val\nVal .equ {value}\n'])
                    self.link(expected=expected)
        self.assemble(['.extrn Val:byte\n.struc S\nf .word[2]\ng .byte\n.ends\nv .S {Val,9}\n',
                       '.public Val\nVal .equ "AB"\n'])
        self.link(expected='41 00 42 00 09')
        self.assemble(['.extrn Val:byte\n.struc S\nf .char[1]\n.ends\nv .S {Val}\n',
                       '.public Val\nVal .equ "AB"\n'])
        self.link(error='storage extent out of range')

    def test_imported_strings_in_union_and_nested_fields(self):
        for value, expected in [('7', '07 00 00 00 09'), ('"AB"', '41 42 00 00 09'),
                                ('"ABCD"', '41 42 43 44 09'), ('""', '00 00 00 00 09')]:
            for nested in (False, True):
                with self.subTest(value=value,nested=nested):
                    types = '.union U\nf .char[4]\ng .word\n.ends\n'
                    use = 'v .U {Val}\n.db 9\n'
                    if nested:
                        types += '.struc S\nf .U\ng .byte\n.ends\n'
                        use = 'v .S {{Val},9}\n'
                    self.assemble(['.extrn Val:byte\n'+types+use, '.public Val\nVal .equ '+value+'\n'])
                    self.link(expected=expected)
        self.assemble(['.extrn Val:byte\n.union U\nf .char[1]\n.ends\nv .U {Val}\n',
                       '.public Val\nVal .equ "AB"\n'])
        self.link(error='storage extent out of range')

    def test_linked_ram_initializers_keep_v1_allocation_spans(self):
        self.assemble(['.dataseg\nVal:.char "AB"\nNext:.byte\n.codeseg\n.dw Val,Next\n'])
        self.link('output{file=out}\nram{start=$10,end=$800}\nlink{file=0.o}\n',expected='10 00 10 00')
        self.assemble(['.dataseg\nVal:.db Missing\n.codeseg\nlda Val\n'])
        self.link('output{file=out}\nram{start=$10,end=$800}\nlink{file=0.o}\n',error='undefined imported')

    def test_leading_ram_storage_has_no_allocation_identity(self):
        for prefix, warnings in [('.dsb 2\n', 0), ('.dsb $+1\n', 0),
                                 ('.dsb $+1\n.dsb $+2\n', 0),
                                 ('scratch: .dsb $+1\n', 1), ('scratch: .dsb 16\n', 1),
                                 ('a1: .dsb 4\na2: .dsb 4\n', 2), ('.db 1,2\n', 0)]:
            for debug in (False, True):
                with self.subTest(prefix=prefix, debug=debug):
                    (self.directory/'0.asm').write_text('.dataseg\n'+prefix+'v .byte\n.codeseg\nlda v\n')
                    r=self.run_tool(self.assembler, ['0.asm','-o','0.o']+(['--debug'] if debug else []))
                    self.assertEqual(r.stderr.count('defined but not used'), warnings, r.stderr)
                    self.link('output{file=out}\nram{start=$200,end=$300}\nlink{file=0.o,origin=$8000}\n',
                              expected='AD 00 02')
        # Storage after a surviving label still contributes to its RAM span.
        self.assemble(['.dataseg\nkeep:.dsb 4\nv .byte\n.codeseg\nlda v\n.dw keep\n'])
        self.link('output{file=out}\nram{start=$200,end=$205}\nlink{file=0.o,origin=$8000}\n',
                  expected='AD 00 02 01 02')
        self.link('output{file=out}\nram{start=$200,end=$204}\nlink{file=0.o,origin=$8000}\n',
                  error='RAM')

    def test_no_ram_blocks_leave_unused_addresses_unplaced(self):
        for public in ('', '.public R\n'):
            for size in ('', '[4]', '[Count]'):
                with self.subTest(public=public,size=size):
                    self.assemble([public+f'.dataseg\nR .byte{size}\n.codeseg\nnop\nrts\nCount .equ 4\n'])
                    self.link('output{file=out}\nlink{file=0.o,origin=$8000}\n',expected='EA 60')
        self.assemble(['.dataseg\nR .byte\n.codeseg\nlda R\n'])
        self.link(error='unresolved')
        self.assemble(['.public R\n.dataseg\nR .byte\n', '.extrn R:byte\nlda R\n'])
        self.link(error='unresolved')
        # No blocks skips placement, not the independent extent checks.
        self.assemble(['.dataseg\nOther:.dsb 1\nR .byte[Other]\n.codeseg\nnop\n', '.codeseg\n'])
        self.link(error='unresolved')
        # Exhausted blocks are different from a script that specified none.
        self.assemble(['.dataseg\nR .byte\nS .byte\n.codeseg\n.dw R,S\n', '.codeseg\n'])
        self.link('output{file=out}\nram{start=$200,end=$201}\nlink{file=0.o}\n',error='RAM')

    def test_export_capture_preserves_unused_equate_warnings_and_errors(self):
        cases=[('.public W\nW .equ 5\n', [(2,'W')]),
               ('.public W\nK .equ 5\nW .equ K\n', [(3,'W')]),
               ('.public W\nW .equ K\nK .equ 5\n', [(2,'W'),(3,'K')]),
               ('.public W\nW .equ Mid\nMid .equ K\nK .equ 5\n', [(2,'W'),(3,'Mid'),(4,'K')]),
               ('.public W\nK .equ 5\nW .equ K+Fwd\nFwd .equ 1\n', [(3,'W'),(4,'Fwd')]),
               ('.public W\nW .equ K\nK .equ 5\n.db W\n', [])]
        for source, warnings in cases:
            for binary in (False,True):
                for option in ('', '--warn-unused-equ', '--Werror=unused-equ'):
                    with self.subTest(source=source,binary=binary,option=option):
                        (self.directory/'t.asm').write_text(source)
                        (self.directory/'out').write_bytes(b'previous output')
                        fails=option=='--Werror=unused-equ' and bool(warnings)
                        args=['t.asm','-o','out']+(['--pure-binary'] if binary else [])+([option] if option else [])
                        r=self.run_tool(self.assembler,args,success=not fails)
                        level='error' if fails else 'warning'
                        expected=''.join(f"t.asm:{line}:1: {level} W0201: symbol '{name}' defined by .EQU is never used\n"
                                         for line,name in warnings) if option else ''
                        self.assertEqual(r.stderr,expected)
                        if fails:
                            self.assertEqual(r.returncode,1)
                            self.assertEqual((self.directory/'out').read_bytes(),b'previous output')
                        else:
                            self.assertNotEqual((self.directory/'out').read_bytes(),b'previous output')
                        self.assertFalse(list(self.directory.glob('*.tmp*')))

    def test_exported_aliases_preserve_otherwise_unused_label_dependencies(self):
        for expr, expected in [('L2','01 80'), ('L2+1','02 80'), ('<L2','01 00'),
                               ('L2-L1','01 00'), ('Alias','02 80')]:
            for before in (False, True):
                with self.subTest(expr=expr, before=before):
                    equates=f'W .equ {expr}\n'+('Alias .equ L2+1\n' if expr=='Alias' else '')
                    body='L1:nop\nL2:rts\n'
                    self.assemble(['.public W\n'+(equates+body if before else body+equates),
                                   '.extrn W:word\n.dw W\n'])
                    self.link('output{file=out}\nlink{file=0.o,origin=$8000}\nlink{file=1.o}\n',
                              expected='EA 60 '+expected)
        self.assemble(['.public W\n.dataseg\nL2:.dsb 4\nW .equ L2\n', '.extrn W:word\n.dw W\n'])
        self.link('output{file=out}\nram{start=$200,end=$204}\nlink{file=0.o}\nlink{file=1.o}\n',
                  expected='00 02')

    def test_reassigned_imports_and_forward_values_keep_each_capture(self):
        for declaration, provider in [('', 'Ext .equ 5\n'), ('.extrn Ext:byte\n', '')]:
            with self.subTest(declaration=declaration):
                body=declaration+'S = Ext\n.db S\nS = S+1\n.db S\nS = S*2\n.db S\n'+provider
                self.assemble([body, '.public Ext\nExt .equ 5\n'])
                self.link(expected='05 06 0C')
        for declaration, provider in [('', 'F:rts\n'), ('.extrn F:word\n', '')]:
            with self.subTest(address=declaration):
                self.assemble([declaration+'S = F\n.dw S\nS = S+1\n.dw S\n'+provider,
                               '.public F\nF:rts\n' if declaration else '.codeseg\n'])
                self.link('output{file=out}\nlink{file=0.o,origin=$8000}\nlink{file=1.o}\n',
                          expected='04 80 05 80 60')
        self.assemble(['.extrn Ext:byte\nS = Ext\nAlias .equ S\nS = S+1\n.db Alias,S\n',
                       '.public Ext\nExt .equ 5\n'])
        self.link(expected='05 06')

    def test_writer_failure_without_location_preserves_output(self):
        # A RAM initializer value is still required even though it emits no
        # bytes. This unplaced address has no usable writer source span yet.
        (self.directory/'in.asm').write_text('.dataseg\n.db $\n')
        (self.directory/'out.o').write_bytes(b'previous output')
        r=self.run_tool(self.assembler, ['in.asm','-o','out.o'], success=False)
        self.assertEqual(r.returncode, 1)
        self.assertIn('location unavailable:0: error:', r.stderr)
        self.assertNotIn('(null)', r.stderr)
        self.assertEqual((self.directory/'out.o').read_bytes(), b'previous output')
        self.assertFalse(list(self.directory.glob('*.tmp*')))

    def test_v1_objects_require_rebuilding(self):
        (self.directory/'0.o').write_bytes(bytes.fromhex('FA CE')+bytes(90))
        self.link(error='rebuild with xasm')

    def test_approved_hex_fixture_round_trips_exactly(self):
        text = (REPO/'design/v2/object-wire-example.hex').read_text()
        # Checked-in hex has commentary following '#'.
        data = bytes.fromhex(' '.join(line.split('#')[0] for line in text.splitlines()))
        self.assertEqual(len(data), 244)
        self.assertEqual(data, encode(*fixture()))
        self.codec_check(data)
        (self.directory/'0.o').write_bytes(data)
        self.link(expected='EA')

    def test_header_boundaries_padding_and_reserved_fields(self):
        good = encode(*fixture())
        for n in range(len(good)):
            with self.subTest(truncate=n): self.codec_check(good[:n], False)
        variants = [good+b'\0', b'XOROBJ1\n'+good[8:], good[:8]+struct.pack('>I', len(good))+good[12:]]
        for offset, value in ((12, 0), (16, 0xffffffff), (92, 0), (96, 0xffffffff), (125, 1), (229, 1)):
            data = bytearray(good)
            if offset in (125, 229): data[offset] = value
            else: struct.pack_into('<I', data, offset, value)
            variants.append(data)
        for i, data in enumerate(variants):
            with self.subTest(mutation=i): self.codec_check(data, False)

    def test_all_expression_tags_and_counted_strings(self):
        for tag in [1, 2, 16, 17, 18, 19, 20, *range(32, 42), *range(44, 50)]:
            with self.subTest(tag=tag):
                expr = [(1, 7, 0, 0, 0, 0), (1, 2, 0, 0, 0, 0)]
                expr.append((tag, 0, 1 if tag >= 32 else 0, 0, 0, 0))
                data = data_object(expr, blobs=[b'A\0B'])
                self.codec_check(data)
                (self.directory/'0.o').write_bytes(data)
                self.link()
        (self.directory/'0.o').write_bytes(data_object([(2, 0, 0, 0, 0, 0)], blobs=[b'A\0B']))
        self.link(expected='41 00 42')
        for tag in (0, 7, 15, 21, 31, 42, 43, 50, 0xffffffff):
            with self.subTest(reserved=tag): self.codec_check(data_object([(tag, 0, 0, 0, 0, 0)]), False)

    def test_invalid_references_opcodes_maps_and_fixup_ownership(self):
        t, blobs = fixture()
        for table, row in ((1,(0,2,0)), (2,(0,0,1,1,1)), (3,(1,3,1,0,1,0)),
                           (6,(5,0,2,1,0)), (8,(3,2,0,0,0,1,0)), (8,(3,0xad,1,0xa9,0,1,0))):
            a = copy.deepcopy(t);a[table][0] = row
            with self.subTest(table=table,row=row): self.codec_check(encode(a,blobs),False)
        for changes in ('missing','duplicate','instruction','map','reference','unused'):
            a=[[] for _ in WIDTHS];a[4]=[(1,1,0,0,0,0)];a[7]=[(0,1,0,0,0,0)];a[8]=[(2,0,1,0,0,0,0)]
            if changes=='missing':a[8]=[]
            if changes=='duplicate':a[8]*=2
            if changes=='instruction':a[8]=[(3,0xa9,0,0,0,0,0)]
            if changes=='map':a[7]=[(0,1,1,0,0,0)]
            if changes=='reference':a[7]=[(1,1,0,0,0,0)]
            if changes=='unused':a[4]=[(1,1,0,7,0,0)]
            with self.subTest(changes=changes):self.codec_check(encode(a,[b'bad map']),False)

    def test_cycles_are_valid_structure_but_fail_if_required(self):
        t=[[] for _ in WIDTHS];t[4]=[(32,0,0,0,0,0)]
        data=encode(t,[]);self.codec_check(data)
        (self.directory/'0.o').write_bytes(data);self.link(expected='')
        t[7]=[(0,1,0,0,0,0)];t[8]=[(2,0,1,0,0,0,0)]
        data=encode(t,[]);self.codec_check(data)
        (self.directory/'0.o').write_bytes(data);self.link(error='circular')

    def test_unused_cyclic_bank_alias_is_not_eagerly_evaluated(self):
        t = [[] for _ in WIDTHS]
        t[3] = [(0,1,0,0,0,0), (1,1,0,1,0,0), (2,1,1,2,0,0)]
        t[4] = [(3,1,0,0,0,0), (3,0,0,0,0,0), (5,0,0,0,0,0)]
        (self.directory/'0.o').write_bytes(encode(t,[b'A',b'B',b'Unused']))
        self.link(expected='')
        t[7] = [(2,1,0,0,0,0)]; t[8] = [(2,0,1,0,0,0,0)]
        (self.directory/'0.o').write_bytes(encode(t,[b'A',b'B',b'Unused']))
        self.link(error='circular')

    def test_large_shared_graph_is_not_expanded_into_a_tree(self):
        count = 20000
        for reverse in (False, True):
            with self.subTest(reverse=reverse):
                if reverse:
                    # Root first forces a deep iterative walk and stack growth.
                    expressions = [(32,i+1,i+1,0,0,0) for i in range(count-1)] + [(1,1,0,0,0,0)]
                    root = 0
                else:
                    expressions = [(1,1,0,0,0,0)] + [(32,i-1,i-1,0,0,0) for i in range(1,count)]
                    root = count-1
                data = data_object(expressions, [(root,4,0,0,0,0)])
                self.codec_check(data)
                (self.directory/'0.o').write_bytes(data)
                self.link(expected='00 00 00 00')

    def test_import_provider_kinds_exports_aliases_and_maps(self):
        (self.directory/'map').write_text('H=1\nI=0\n')
        self.assemble(['.extrn S,N,P:byte\n.charmap "map"\n.char S,"HI"\nlda #N\njsr P\n',
                       '.public S,N,P\nS .equ Text\nText .equ "HI"\nN .equ 7\nP:rts\n'])
        self.link(expected='01 00 01 00 A9 07 20 09 00 60')
        self.assemble(['.public Shared\nShared .equ 1\n','.public Shared\nShared .equ 2\n'])
        self.link(error="duplicate exported symbol `Shared'")
        self.assemble(['lda Missing\n','nop\n'])
        self.link(error="undefined imported symbol `Missing'")

    def test_script_layout_errors_are_located_and_preserve_output(self):
        self.assemble(['nop\n'])
        (self.directory/'folder').mkdir()
        for tail, message, line in (
                ('copy{file=missing.bin}\n', "`copy': could not read `missing.bin'", 3),
                ('copy{file=folder}\n', "`copy': could not read `folder'", 3),
                ('link{file=0.o,origin=$10000}\n', "value of argument `origin' is out of range", 3)):
            with self.subTest(tail=tail):
                r=self.link('output{file=out}\nlink{file=0.o}\n'+tail, error=message)
                self.assertEqual(r.returncode, 1)
                self.assertIn(f'link.s:{line}: error:', r.stderr)
                self.assertNotIn('no error', r.stderr)

    def test_script_numbers_require_the_complete_numeric_token(self):
        self.assemble(['Here:.dw Here\n'])
        for text in ('8000h', '$C000x', '1e3', '$', '%', '%12'):
            with self.subTest(text=text):
                r=self.link('output{file=out}\nlink{file=0.o,origin='+text+'}\n',
                            error="invalid value of `origin'")
                self.assertIn('link.s:2: error:', r.stderr)
        for text, expected in (('8000', '40 1F'), ('$8000', '00 80'),
                               ('0x8000', '00 80'), ('%100', '04 00')):
            with self.subTest(text=text):
                self.link('output{file=out}\nlink{file=0.o,origin='+text+'}\n', expected=expected)

    def test_link_heap_failures_report_real_errors_and_keep_output(self):
        self.assemble(['.extrn Val:byte\n'+'.db Val\n'*300,
                       '.public Val\nVal .equ 7\n'])
        (self.directory/'link.s').write_text('output{file=out}\nlink{file=0.o}\nlink{file=1.o}\n')
        for at in range(1, 129):
            (self.directory/'out').write_bytes(b'previous output')
            env=dict(os.environ, XASM_TEST_LINK_HEAP_FAIL=str(at))
            r=subprocess.run([str(self.linker), 'link.s'], cwd=self.directory,
                             env=env, capture_output=True, text=True, timeout=30)
            self.assertNotRegex(r.stderr, r'AddressSanitizer|LeakSanitizer|runtime error:|no error')
            self.assertIn(r.returncode, (0,1), r.stderr)
            if not r.returncode:
                self.assertGreater(at, 2)
                self.assertEqual((self.directory/'out').read_bytes(), bytes([7])*300)
                break
            self.assertIn('error:', r.stderr)
            self.assertIn('memory', r.stderr)
            self.assertEqual((self.directory/'out').read_bytes(), b'previous output')
            self.assertEqual(list(self.directory.glob('.xasm-*')), [])
        else:
            self.fail('mapping-failure sweep did not reach a successful link')

    def test_bank_references_keep_targets_when_interleaved_and_reused(self):
        count=2000
        self.assemble(['.extrn Far,Near:word\nAlias .equ ^Far\n'+
                       '.db ^Far, <Near, ^Near, Alias\n'*count,
                       '.public Far\nFar:rts\n', '.public Near\nNear:rts\n'])
        self.link('output{file=out}\nbank{origin=$8000,size=8192}\nlink{file=0.o}\n'
                  'bank{size=1}\nlink{file=1.o}\nbank{}\nlink{file=2.o}\n')
        self.assertEqual((self.directory/'out').read_bytes(),
                         bytes([1,0,2,1])*count+bytes(8192-4*count)+b'\x60\x60')

    def test_forward_strings_per_operand_pc_and_zero_field_remainder(self):
        self.assemble(['.extrn S:byte\n.dw S,$\n','S .equ "AB"\n.public S\n'])
        self.link(expected='41 00 42 00 04 00')
        self.assemble(['.extrn N:byte\n.struc S\nnm .char[N]\nfb .byte\n.ends\nv .S {"AB",7}\n.db $FF\n',
                       'N .equ 2\n.public N\n'])
        self.link(expected='41 42 07 FF')
        self.assemble(['.extrn S:byte\n.db S+0\n','S .equ "A"\n.public S\n'])
        self.link(expected='41 30')

    def test_narrowing_spans_and_range_policy(self):
        for k,want in ((263,'EA A5 00 AD 00 01 EA 60'),(262,None)):
            with self.subTest(k=k):
                self.assemble([f'K .equ {k}\nBack:nop\nlda Back\nlda K-(L-Back)\nnop\nL:rts\n'])
                self.link(expected=want,error='operand out of range' if want is None else None)
        self.assemble(['.dsb Tail-Head\nHead:nop\nnop\nTail:rts\n'])
        self.link(expected='00 00 EA EA 60')
        self.assemble(['bne Target\n.dsb 128\nTarget:nop\n'])
        r=self.link(error='operand out of range');self.assertIn('0.asm:1:',r.stderr)
        self.assemble(['jmp L+$10000\nL:rts\n'])
        self.link(error='operand out of range')

    def test_ram_exact_order_spans_and_failed_alignment_split(self):
        self.assemble(['.dataseg\nbig .byte[4]\nsmall .byte\n.codeseg\n.dw big,small\n',
                       '.dataseg\ntiny .byte\n.codeseg\n.dw tiny\n'])
        self.link('output{file=out}\nram{start=$10,end=$800}\nlink{file=0.o}\nlink{file=1.o}\n',expected='12 00 10 00 11 00')
        (self.directory/'1.o').unlink()
        self.assemble(['.dataseg\nbuf .byte[256]\n.align buf 256\nbig .byte[$120]\n.codeseg\n.dw buf,big\n'])
        script='output{file=out}\nram{start=$310,end=$490}\nram{start=$800,end=$A00}\n'
        self.link(script+'ram{start=$1000,end=$1200}\nlink{file=0.o}\n',expected='00 08 00 10')
        self.link(script+'link{file=0.o}\n',error='RAM placement failed')

    def test_banks_large_chr_copy_and_code_address_limit(self):
        self.assemble(['.extrn Far:word\nAlias .equ Far\nlda #^Alias\n','.public Far\nFar:rts\n'])
        (self.directory/'chr').write_bytes(bytes(range(256))*512)
        script='output{file=out}\nbank{origin=$8000,size=4}\nlink{file=0.o}\nbank{}\nlink{file=1.o}\nbank{size=$20000}\ncopy{file=chr}\n'
        self.link(script)
        self.assertEqual((self.directory/'out').read_bytes(),bytes.fromhex('A9 01 00 00 60 00 00 00')+(self.directory/'chr').read_bytes())
        self.assemble(['nop\nnop\n'])
        (self.directory/'1.o').unlink()
        self.link('output{file=out}\nlink{file=0.o,origin=$FFFF}\n',error='layout exceeds')

    def test_script_output_switches_from_command_line_output(self):
        for prefix in ('', 'pad{size=3}\n'):
            with self.subTest(prefix=prefix):
                (self.directory/'link.s').write_text(prefix + 'output{file=second}\npad{size=5}\n')
                (self.directory/'first').write_bytes(b'previous first')
                (self.directory/'second').write_bytes(b'previous second')
                self.run_tool(self.linker, ['-o', 'first', 'link.s'])
                self.assertEqual((self.directory/'first').read_bytes(), bytes(3 if prefix else 0))
                self.assertEqual((self.directory/'second').read_bytes(), bytes(5))

    def test_linker_stages_all_outputs_before_publication(self):
        (self.directory/'link.s').write_text('output{file=first}\npad{size=3}\noutput{file=second}\npad{size=5}\n')
        for operation in ('create', 'flush', 'permissions', 'close', 'rename'):
            for target in ('first', 'second'):
                with self.subTest(operation=operation, target=target):
                    for name in ('first', 'second'): (self.directory/name).write_bytes(b'previous')
                    environment = dict(os.environ, XLNK_TEST_FAULT=operation, XLNK_TEST_OUTPUT=target)
                    r = self.run_tool(self.faultlinker, ['link.s'], success=False, env=environment)
                    self.assertIn('INJECT '+operation, r.stderr)
                    self.assertEqual((self.directory/'first').read_bytes(),
                                     bytes(3) if operation == 'rename' and target == 'second' else b'previous')
                    self.assertEqual((self.directory/'second').read_bytes(), b'previous')
                    self.assertEqual(list(self.directory.glob('.xasm-*')), [])

    def test_duplicate_new_output_paths_are_rejected_before_creation(self):
        (self.directory/'sub').mkdir()
        (self.directory/'alias').symlink_to('sub', target_is_directory=True)
        for spelling in ('./out', 'sub/../out', 'alias/../out'):
            with self.subTest(spelling=spelling):
                (self.directory/'link.s').write_text('output{file=out}\npad{size=3}\noutput{file='+spelling+'}\npad{size=5}\n')
                r = self.run_tool(self.linker, ['link.s'], success=False)
                self.assertIn('duplicate output', r.stderr)
                self.assertFalse((self.directory/'out').exists())
                self.assertEqual(list(self.directory.glob('.xasm-*')), [])

    def test_new_output_names_follow_filesystem_case_and_normalization(self):
        for first, second in (('out', 'OUT'), ('caf\u00e9', 'cafe\u0301'), ('K', '\u212a')):
            with self.subTest(first=first, second=second):
                # Probe this volume's identity rules; Linux case-sensitive
                # directories keep both names, macOS commonly aliases them.
                (self.directory/first).write_bytes(b'probe')
                same = (self.directory/second).exists()
                (self.directory/first).unlink()
                (self.directory/'link.s').write_text(
                    'output{file='+first+'}\npad{size=3}\noutput{file='+second+'}\npad{size=5}\n')
                r = self.run_tool(self.linker, ['link.s'], success=not same)
                if same:
                    self.assertIn('duplicate output', r.stderr)
                    self.assertFalse((self.directory/first).exists())
                    self.assertFalse((self.directory/second).exists())
                else:
                    self.assertEqual((self.directory/first).read_bytes(), bytes(3))
                    self.assertEqual((self.directory/second).read_bytes(), bytes(5))
                    (self.directory/first).unlink()
                    (self.directory/second).unlink()
                self.assertEqual(list(self.directory.glob('.xasm-*')), [])

    def test_object_publication_errors_and_output_aliases(self):
        for src,error in (('.org $8000\n.db 1\n','org'),('nop\n.if $==1\n.db 1\n.endif\n','before final layout')):
            (self.directory/'in.asm').write_text(src);(self.directory/'kept.o').write_bytes(b'previous')
            r=self.run_tool(self.assembler,['in.asm','-o','kept.o'],False)
            self.assertIn(error,r.stderr);self.assertEqual((self.directory/'kept.o').read_bytes(),b'previous')
        self.assemble(['nop\n'])
        original=(self.directory/'0.o').read_bytes()
        (self.directory/'alias').symlink_to('0.o')
        (self.directory/'link.s').write_text('output{file=alias}\nlink{file=0.o}\n')
        self.run_tool(self.linker,['link.s'],False)
        self.assertEqual((self.directory/'0.o').read_bytes(),original)


if __name__ == '__main__':
    unittest.main()
