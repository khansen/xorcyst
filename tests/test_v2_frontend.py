#!/usr/bin/env python3
"""I3 source-to-owned-layout controls; installed tools still use v1."""
import os
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]

def build(destination, provenance=False):
    sources = re.search(r'^xasm_SOURCES = (.*?)(?=\n\n)', (REPO/'Makefile.am').read_text(), re.M|re.S)[1]
    sources = [REPO/('v2/frontend.c' if word=='astproc.c' else word)
               for word in sources.replace('\\\n',' ').split() if word.endswith('.c')]
    if provenance:
        sources = [REPO/'tests/test_v2_source_driver.c' if p.name == 'xasm.c' else p for p in sources]
    command = [*shlex.split(os.environ.get('CC','cc')), '-I',str(REPO),'-Wall','-DXORCYST_V2_FRONTEND',
               *shlex.split(os.environ.get('TEST_V2_CFLAGS','-O2 -g -Werror')), *map(str,sources),
               str(REPO/'v2/heap.c'),str(REPO/'v2/value.c'),str(REPO/'v2/layout.c'),str(REPO/'v2/program.c'),str(REPO/'v2/object.c'),'-o',str(destination)]
    result=subprocess.run(command,capture_output=True,text=True,timeout=120)
    if result.returncode: raise AssertionError(result.stderr)

class Frontend(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        directory=tempfile.TemporaryDirectory(prefix='xorcyst-i3-build-')
        cls.addClassCleanup(directory.cleanup)
        cls.tool=Path(directory.name)/'xasm-v2'
        build(cls.tool)
        cls.provenance_tool = Path(directory.name)/'xasm-v2-provenance'
        build(cls.provenance_tool, provenance=True)
        cls.source_locations = Path(directory.name)/'source-locations'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-I', str(REPO),
                   '-DXORCYST_V2_FRONTEND', '-Wall',
                   *shlex.split(os.environ.get('TEST_V2_CFLAGS', '-O2 -g -Werror')),
                   str(REPO/'tests/test_v2_source_locations.c'), '-o', str(cls.source_locations)]
        r = subprocess.run(command, capture_output=True, text=True)
        if r.returncode: raise AssertionError(r.stderr)
        cls.protection = Path(directory.name)/'protection'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-I', str(REPO),
                   '-DXORCYST_V2_FRONTEND', '-Wall',
                   *shlex.split(os.environ.get('TEST_V2_CFLAGS', '-O2 -g -Werror')),
                   *[str(REPO/p) for p in ('tests/test_v2_dependencies.c', 'output_file.c', 'sha256.c')],
                   '-o', str(cls.protection)]
        r = subprocess.run(command, capture_output=True, text=True)
        if r.returncode: raise AssertionError(r.stderr)

    def setUp(self):
        directory=tempfile.TemporaryDirectory(prefix='xorcyst-i3-source-')
        self.addCleanup(directory.cleanup)
        self.directory=Path(directory.name)

    def assemble(self, source, expected=None, error=None, options=(), error_code=1):
        (self.directory/'in.asm').write_text(source)
        (self.directory/'out').write_bytes(b'previous output')
        r=subprocess.run([str(self.tool),'--pure-binary','in.asm','-o','out',*options],
                         cwd=self.directory,capture_output=True,text=True,timeout=15)
        self.assertNotRegex(r.stderr,r'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:')
        self.assertGreaterEqual(r.returncode,0,r.stderr)
        if error:
            self.assertEqual(r.returncode,error_code,r.stderr)
            self.assertIn(error,r.stderr)
            if not options:self.assertRegex(r.stderr,r'in.asm:\d+: error:')
            self.assertEqual((self.directory/'out').read_bytes(),b'previous output')
        else:
            self.assertEqual(r.returncode,0,r.stderr)
            if expected is not None:self.assertEqual((self.directory/'out').read_bytes(),bytes.fromhex(expected))
        self.assertFalse((self.directory/'out.tmp').exists())
        return r

    def run_source_locations(self, case):
        r = subprocess.run([str(self.source_locations), case], capture_output=True, text=True, timeout=15)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stderr, '')

    def test_plain_nodes_do_not_allocate_rich_source_spans(self):
        self.run_source_locations('plain')

    def test_parsed_source_span_survives_nested_expansion_and_original_free(self):
        self.run_source_locations('expanded')

    def test_provenance_and_frontend_cache_ids_have_independent_lifetimes(self):
        self.run_source_locations('identities')

    def test_optional_source_span_allocation_failure_keeps_allocator_usable(self):
        self.run_source_locations('allocation')

    def test_source_capture_preserves_bytes_and_macro_definition_spans(self):
        (self.directory/'map.txt').write_text('H=1\nI=0\n')
        source = ('.charmap "map.txt"\n.macro M p\n.db "A"+p\n.char "HI","HI"\n.endm\n'
                  'M "B"\nM "C"\n.db S+"D"\nS .equ "X"\n')
        self.assemble(source, '41 42 01 00 01 00 41 43 01 00 01 00 58 44')
        r = subprocess.run([str(self.provenance_tool), '--pure-binary', 'in.asm', '-o', 'captured'],
                           cwd=self.directory, capture_output=True, text=True, timeout=15)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual((self.directory/'captured').read_bytes(), (self.directory/'out').read_bytes())
        captures = [tuple(map(int, x.split())) for x in re.findall(r'CAPTURE (\d+ \d+)', r.stderr)]
        self.assertEqual(captures, [(6,3),(6,4),(7,3),(7,4),(8,8)])
        self.assertNotRegex(r.stderr, r'error:|AddressSanitizer|runtime error:')

    def test_output_protection_keeps_missing_probes_and_unicode_aliases(self):
        def compare(a, b, aliases, env=None):
            r = subprocess.run([str(self.protection), a, b], cwd=self.directory,
                               capture_output=True, text=True, env=env)
            self.assertEqual(r.returncode, 3 if aliases else 0, r.stderr)
            self.assertNotRegex(r.stderr, r'AddressSanitizer|runtime error:')
            if aliases: self.assertIn('output protection', r.stderr)
            else: self.assertEqual(r.stderr, '')
            return r
        (self.directory/'existing').write_text('keep')
        compare('existing', 'missing', False)
        compare('missing', 'existing', False)
        compare('missing', './missing', True)
        compare('missing', 'different', False)
        (self.directory/'dir').mkdir()
        (self.directory/'alias').symlink_to('dir', target_is_directory=True)
        compare('dir/missing', 'alias/missing', True)
        os.link(self.directory/'existing', self.directory/'hard')
        compare('existing', 'hard', True)
        if sys.platform == 'darwin':
            compare('café.o', 'café.o', True)
            unavailable = {**os.environ, 'TEST_V2_UNICODE_UNAVAILABLE': '1'}
            compare('left', 'right', False, unavailable)
            compare('left', 'left-longer', False, unavailable)
            compare('left-longer', 'left', False, unavailable)
            # Query the volume through actual existing-file lookup, separately
            # from the production comparison of not-yet-existing names.
            (self.directory/'CASE-PROBE').write_text('probe')
            insensitive = (self.directory/'case-probe').exists()
            compare('MixedCase', 'mixedcase', insensitive, unavailable)
            compare('K.o', 'K.o', True)
            r = compare('café.o', 'café.o', True, unavailable)
            self.assertIn('cannot initialize Unicode output comparison', r.stderr)
        self.assertEqual((self.directory/'existing').read_text(), 'keep')

    def test_narrowing_controls(self):
        for k,want in ((263,'EA A5 00 AD 00 01 EA 60'),(262,None)):
            for expr in ('K-L','K-(L-Back)'):
                with self.subTest(k=k,expr=expr):
                    self.assemble(f'K .equ {k}\nBack: nop\nlda Back\nlda {expr}\nnop\nL: rts\n',want,
                                  error='operand out of range' if want is None else None)
        self.assemble('.org $F6\nlda L\nlda M\nM: nop\nnop\nnop\nnop\nL: rts\n','A5 FE A5 FA EA EA EA EA 60')
        self.assemble('.org $FA\nM .equ $10\nlda M\nlda L\nnop\nL: rts\n','A5 10 AD 00 01 EA 60')
        self.assemble('lda T-3\nT:nop\n',error='operand out of range')
        self.assemble('Z .equ $10\nlda Z\nlda T-6\nT:nop\n','A5 10 AD FF FF EA')
        self.assemble('.dataseg\n.org $10\nv .byte\n.codeseg\n.org $8000\nloop:\n'+'lda v\n'*43+'bne loop\n','A5 10'*43+'D0 A8')

    def test_strings_maps_operand_pc_and_kinds(self):
        (self.directory/'map.txt').write_text('H=1\nI=0\n')
        for directive,expected in (('.db','41 42 02'),('.dw','41 00 42 00 04 00'),('.dd','41 00 00 00 42 00 00 00 08 00 00 00')):
            with self.subTest(directive=directive):self.assemble(f'{directive} Text,$\nText .equ "AB"\n',expected)
        self.assemble('.db S+0\nS .equ "A"\n','41 30')
        for expression in ('"abc"*1','"abc"/1','1*"abc"','S*1'):
            self.assemble(f'.db {expression}\nS .equ "abc"\n',error='operand type')

    def test_source_storage_spans_and_fields(self):
        self.assemble('.dsb Tail-Head\nHead:nop\nnop\nTail:rts\n','00 00 EA EA 60')
        self.assemble('Head:.dsb Tail-Head\nTail:rts\n',error='circular')
        self.assemble('.struc S\nf .word\n.ends\nv .S\n.word v\n','00 00 00 00')
        self.assemble('.struc S\nf .byte[N]\ng .byte\n.ends\nv .S {"AB",7}\nN .equ 2\n.byte $FF\n','41 42 07 FF')
        for source in ('.dsw 32769\n','.dsd $40000001\n','.dsb -1\n'):
            self.assemble(source,error='operand out of range')

    def test_binding_capture_and_scopes(self):
        self.assemble('.db I\nI = 1\nI = 2\n.db I\nI = 3\n.db I\n','03 02 03')
        self.assemble('.db Q\n.define Q 1\n.undef Q\n.define Q 2\n','02')
        self.assemble('.org $8000\nFirst:\n@@x: .db 1\nAlias = @@x\nSecond:\n@@x: .db 2\nlda Alias\n','01 02 AD 00 80')
        self.assemble('.proc P\nlda #1\nrts\n.endp\njsr P\n','A9 01 60 20 00 00')
        self.assemble('b2 .byte[4]\nlda b2[2]\n','00 00 00 00 A5 02')

    def test_reassignment_captures_new_deferred_definition_after_rhs(self):
        cases = [
            ('S = K\nS = S+1\n.db S\nK .equ 5\n', '06'),
            ('S = K\n.db S\nS = S+1\n.db S\nS = S*2\n.db S\nK .equ 5\n', '05 06 0C'),
            ('S = K\nAlias .equ S\nS = S+1\n.db Alias,S\nK .equ 5\n', '05 06'),
            ('S = F\n.dw S\nS = S+1\n.dw S\nF:rts\n', '04 00 05 00 60'),
            ('S = K\n.char S\nS = S+1\n.char S\nS = S+"B"\n.char S\nK .equ "A"\n',
             '41 41 31 41 31 42'),
        ]
        for source, expected in cases:
            for debug in (False, True):
                with self.subTest(source=source, debug=debug):
                    self.assemble(source, expected, options=('--debug',) if debug else ())

    def test_undef_redefinitions_keep_distinct_declarations_and_final_name_binding(self):
        cases=[('lda T\nT:.db 1\n.undef T\nT:.db 2\n','A5 03 01 02'),
               ('jsr T\n.proc T\n.db 1\n.endp\n.undef T\n.proc T\n.db 2\n.endp\n','20 04 00 01 02'),
               ('lda T\nT .byte 1\n.undef T\nT .byte 2\n','A5 03 01 02'),
               ('.dataseg\nT:.dsb 2\n.undef T\nT:.dsb 3\n.codeseg\nlda T\n','A5 02'),
               ('.dataseg\nT .byte[2]\n.undef T\nT .byte[3]\n.codeseg\nlda T\n','A5 02'),
               ('T:.db 1\nAlias .equ T+1\nlda Alias\n.undef T\nT:.db 2\n','01 A5 04 02'),
               ('T:.db 1\nAlias .equ T+1\n.db Alias\n.undef T\nT:.db 2\n','01 03 02'),
               ('Alias .equ T+1\nlda Alias\nT:.db 1\n.undef T\nT:.db 2\n','A5 04 01 02'),
               ('T:.db 1\nAlias .equ T+1\n.undef T\nT .equ 7\n.db Alias\n','01 08')]
        for source,expected in cases:
            for debug in (False,True):
                with self.subTest(source=source,debug=debug):
                    self.assemble(source,expected,options=('--debug',) if debug else ())

    def test_nesrev_pair_redefined_label_contract_fixture(self):
        # The binding fixture named by D0/D1, kept self-contained for CI.
        source='''ZP_TestPtrLo .EQU $20
ZP_TestPtrHi .EQU $21
.ORG $8000
LoadRawPointer:
  LDY #0
  LDA RawPointerTable,Y
  STA ZP_TestPtrLo
  LDA RawPointerTable+1,Y
  STA ZP_TestPtrHi
  LDA [ZP_TestPtrLo],Y
  RTS
RawPointerTable: .DB $00,$81,$02,$81,$04,$81,$06,$81,$08,$81,$0A,$81
.ORG $8100
TargetData: .DB 1,2,3,4,5,6,7,8,9,10,11,12
.UNDEF RawPointerTable
.ORG $8200
RawPointerTable: .DB $00,$81,$02,$81,$04,$81,$06,$81,$08,$81,$0A,$81
'''
        table='00 81 02 81 04 81 06 81 08 81 0A 81'
        for debug in (False,True):
            with self.subTest(debug=debug):
                self.assemble(source,'A0 00 B9 00 82 85 20 B9 01 82 85 21 B1 20 60 '+table+
                              ' 01 02 03 04 05 06 07 08 09 0A 0B 0C '+table,
                              options=('--debug',) if debug else ())

    def test_sizeof_rechecks_label_kinds_after_undef(self):
        for operand,alias in (('L',''),('L+0',''),('A1','A1 .equ L\n'),('A1','A1 .equ L+0\n')):
            for directive in ('.db','.dw','.dd'):
                for text in ('','A','ABCD'):
                    source=f'L:nop\n{alias}Tbl: {directive} {operand}\n.db sizeof(Tbl)\n.undef L\nL .equ "{text}"\n'
                    line=source.splitlines().index('.db sizeof(Tbl)')+1
                    with self.subTest(operand=operand,alias=alias,directive=directive,text=text):
                        r=self.assemble(source,error="size of `Tbl' is unknown")
                        self.assertIn(f'in.asm:{line}: error:',r.stderr)
        # When the string is already known at sizeof, the scan uses its length.
        self.assemble('L:nop\nTbl:.dw L\n.undef L\nL .equ "ABCD"\n.db sizeof(Tbl)\n',
                      'EA 41 00 42 00 43 00 44 00 09')

    def test_source_time_pc_rechecks_label_data_widths_after_undef(self):
        uses=('.if $ == 3\n.db $33\n.endif',
              'Here .equ $\n.dw Here','.define Here $\n.dw Here','Here = $\n.dw Here',
              '.org $+1','.rept $\n.db 7\n.endm','.message $')
        for operand,alias in (('L',''),('L+0',''),('A1','A1 .equ L\n'),('A1','A1 .equ L+0\n')):
            for use in uses:
                source=f'L:nop\n{alias}.dw {operand}\n{use}\n.undef L\nL .equ "ABCD"\n'
                line=3+alias.count('\n')
                for debug in (False,True):
                    with self.subTest(operand=operand,alias=alias,use=use,debug=debug):
                        r=self.assemble(source,error='cannot use $ before final layout',
                                        options=('--debug',) if debug else ())
                        self.assertIn(f'in.asm:{line}: error:',r.stderr)

    def test_sizeof_rechecks_the_counted_kind_and_string_length(self):
        # Forward data binds the final name; sizeof must validate what it counted
        # even when the name has a known value at the intervening sizeof use.
        for directive in ('.db', '.dw', '.dd', '.char'):
            for before,after in (('"ABCD"','7'), ('7','"ABCD"'),
                                 ('"HELLO"','"HI"'), ('""','"A"'),
                                 ('"A"','""'), ('"A"','7'), ('7','"A"')):
                for debug in (False,True):
                    source=f'Tbl: {directive} S\nS = {before}\n.byte sizeof(Tbl)\nS = {after}\n'
                    with self.subTest(directive=directive,before=before,after=after,debug=debug):
                        r=self.assemble(source,error="size of `Tbl' is unknown",
                                        options=('--debug',) if debug else ())
                        self.assertIn('in.asm:3: error:',r.stderr)
        source='L:nop\nTbl:.dw L\n.undef L\nL .equ "ABCD"\n.db sizeof(Tbl)\n.undef L\nL:rts\n'
        r=self.assemble(source,error="size of `Tbl' is unknown")
        self.assertIn('in.asm:5: error:',r.stderr)
        # Numeric changes and same-length strings keep their measured extent.
        self.assemble('Tbl:.dw S\nS=7\n.byte sizeof(Tbl)\nS=9\n','09 00 03')
        self.assemble('Tbl:.dw S\nS="AB"\n.byte sizeof(Tbl)\nS="CD"\n','43 00 44 00 05')
        self.assemble('Msg:.char S\nS .equ "HELLO"\n.byte sizeof(Msg)\n','48 45 4C 4C 4F 06')
        self.assemble('Tbl:.db S\nS=""\n.byte sizeof(Tbl)\nS=""\n','01')
        # Values already captured at the data line are independent of later names.
        self.assemble('S="AB"\nTbl:.dw S\nS=7\n.byte sizeof(Tbl)\n','41 00 42 00 05')
        self.assemble('S=7\nTbl:.dw S\nS="AB"\n.byte sizeof(Tbl)\n','07 00 03')
        self.assemble('.proc P\nTbl:.dw S\nS="AB"\n.byte sizeof(Tbl)\nS="CD"\n.endp\n',
                      '43 00 44 00 05')

    def test_backward_local_data_allows_source_time_pc(self):
        for directive,operand,value,pc in (('.db','@@l','01',3),
                                           ('.dw','@@l','01 00',4),
                                           ('.dw','@@l+1','02 00',4)):
            body=f'@@l:nop\n{directive} {operand}\n.if $=={pc}\n.db $33\n.endif\n'
            for macro in (False,True):
                source=('.macro Emit\n'+body+'.endm\nfoo:nop\nEmit\n' if macro else 'foo:nop\n'+body)
                for debug in (False,True):
                    with self.subTest(directive=directive,operand=operand,macro=macro,debug=debug):
                        self.assemble(source,'EA EA '+value+'33',options=('--debug',) if debug else ())

    def test_sizeof_keeps_captures_later_in_the_data_statement(self):
        for directive,width in (('.db',1),('.char',1),('.dw',2),('.dd',4)):
            for before,values,after in (('"AB"',[65,66],'7'),('5',[5],'"ABCD"'),
                                        ('"AB"',[65,66],'"ABCD"'),('""',[],'"A"')):
                for equ in (False,True):
                    declaration=f'S .equ {before}\n' if equ else f'S = {before}\n'
                    replacement=f'.undef S\nS .equ {after}\n' if equ else f'S = {after}\n'
                    for operands,repeats in (('sizeof(Tbl), S',1),('S, sizeof(Tbl), S',2)):
                        size=(1+len(values)*repeats)*width
                        numbers=([size]+values if repeats==1 else values+[size]+values)
                        expected=b''.join(n.to_bytes(width,'little') for n in numbers).hex()
                        for debug in (False,True):
                            with self.subTest(directive=directive,before=before,after=after,equ=equ,
                                              operands=operands,debug=debug):
                                self.assemble(declaration+f'Tbl: {directive} {operands}\n'+replacement,
                                              expected,options=('--debug',) if debug else ())
        self.assemble('S="AB"\nTbl:.db sizeof(Tbl)+0,S+"!"\nS=7\n','04 41 42 21')
        # The scan can also precede capture in a following data statement.
        self.assemble('S="AB"\nTbl:.db sizeof(Tbl)\n.db S\nS=7\n','03 41 42')
        for wrapper in ('.macro Emit\n%s.endm\nEmit\n','.rept 1\n%s.endm\n','.proc P\n%s.endp\n',
                        '.if 1\n%s.endif\n','I=0\n.while I<1\n%sI=I+1\n.endm\n',
                        'I=0\n.do\n%sI=I+1\n.until I==1\n',
                        '.macro Emit\n%s.exitm\n.db 9\n.endm\nEmit\n'):
            self.assemble('S="AB"\n'+wrapper%'Tbl:.dw sizeof(Tbl),S\n'+'S=7\n',
                          '06 00 41 00 42 00')
        # Error cleanup must not inspect a statement left with pending guards.
        self.assemble('S="AB"\nTbl:.db sizeof(Tbl),S\n.error "stop"\n',error='stop')

    def test_sizeof_user_data_replacement_keeps_a_located_rejection(self):
        for fields,initializer in (('f .byte\n','5'),('f .byte\n','K'),
                                   ('f .word\ng .byte\n','K,2')):
            definition='.struc S\n'+fields+'.ends\n'
            for directive in ('.tag S','.S'):
                for gap in ('','.db 7\n'):
                    body=f'Tbl:.db sizeof(Tbl)\n{gap}{directive} {{ {initializer} }}\n'
                    for wrapper in ('%s','.macro Emit\n%s.endm\nEmit\n',
                                    '.proc P\n%s.endp\n','.rept 1\n%s.endm\n'):
                        source=definition+wrapper%body+'K .equ 5\n'
                        line=source.splitlines().index('Tbl:.db sizeof(Tbl)')+1
                        if wrapper.startswith('.macro'):
                            line=source.splitlines().index('Emit')+1
                        for debug in (False,True):
                            with self.subTest(fields=fields,directive=directive,gap=gap,
                                              wrapper=wrapper,debug=debug):
                                r=self.assemble(source,error="size of `Tbl' is unknown",
                                                options=('--debug',) if debug else ())
                                self.assertIn(f'in.asm:{line}: error:',r.stderr)
        # A later native statement cannot accidentally bind an aggregate's guard.
        self.assemble('.struc S\nf .byte\n.ends\nTbl:.db sizeof(Tbl)\n.tag S {5}\n.db K\nK=7\n',
                      error="size of `Tbl' is unknown")
        # Processing the aggregate before the scan remains supported.
        self.assemble('.struc S\nf .byte\n.ends\nTbl:.tag S {K}\n.db sizeof(Tbl)\nK=5\n','05 02')
        self.assemble('.struc S\nf .word\ng .byte\n.ends\nTbl:.S {K,2}\n.db sizeof(Tbl)\nK=5\n',
                      '05 00 02 04')
        r=self.assemble('Tbl:.db sizeof(Tbl)\n.tag Nope {5}\n',error="unknown type `Nope'")
        self.assertIn('in.asm:2: error:',r.stderr)

    def test_sizeof_guards_bind_each_native_statement_and_each_use(self):
        # Distinct future statements have independent pending lists and widths.
        self.assemble('S="AB"\nTbl:.db sizeof(Tbl)\n.db S\n.dw T\nT=7\nS=5\n',
                      '05 41 42 07 00')
        # Two scans can both need the same operand after its own reduction.
        self.assemble('S="AB"\nTbl:.db sizeof(Tbl)+sizeof(Tbl),S\nS=7\n','06 41 42')
        self.assemble('Tbl:.db sizeof(Tbl)+sizeof(Tbl),K\nK=7\n','04 07')
        self.assemble('L:nop\nTbl:.dw L\n.db sizeof(Tbl)\n','EA 00 00 03')
        self.assemble('.macro Emit\nTbl:.db sizeof(Tbl),S\n.undef Tbl\n.endm\nS="AB"\nEmit\nS="C"\nEmit\nS=7\n',
                      '03 41 42 02 43')

    def test_sizeof_anonymous_references_keep_their_numeric_width(self):
        cases=[('Tbl:.db sizeof(Tbl)\n.dw +\n+ nop\n','03 03 00 EA'),
               ('Tbl:.dw sizeof(Tbl),+\n+ nop\n','04 00 04 00 EA'),
               ('- nop\nTbl:.dw sizeof(Tbl),-\n','EA 04 00 00 00'),
               ('- nop\nTbl:.db sizeof(Tbl)\n.dw -\n','EA 03 00 00'),
               ('Tbl:.dw +\n.db sizeof(Tbl)\n+ nop\n','03 00 03 EA'),
               ('- nop\nTbl:.dw -\n.db sizeof(Tbl)\n','EA 00 00 03'),
               ('Tbl:.db sizeof(Tbl)\n.dw ++\n++ nop\n','03 03 00 EA'),
               ('-- nop\nTbl:.db sizeof(Tbl)\n.dw --\n','EA 03 00 00')]
        for body,expected in cases:
            for wrapper in ('%s','.macro Emit\n%s.endm\nEmit\n',
                            '.proc P\n%s.endp\n','.rept 1\n%s.endm\n'):
                for debug in (False,True):
                    with self.subTest(body=body,wrapper=wrapper,debug=debug):
                        self.assemble(wrapper%body,expected,options=('--debug',) if debug else ())

    def test_repeated_sizeof_operands_do_not_reenter_the_scan(self):
        for directive,width in (('.db',1),('.dw',2),('.dd',4)):
            for body,numbers in (
                    (f'Tbl:{directive} sizeof(Tbl),sizeof(Tbl)\n',[2*width,2*width]),
                    (f'Tbl:{directive} sizeof(Tbl)\n{directive} 1\n{directive} sizeof(Tbl)\n',
                     [3*width,1,3*width]),
                    (f'Tbl:{directive} sizeof(Tbl)+sizeof(Tbl),sizeof(Tbl)\n',[4*width,2*width]),
                    (f'Tbl:{directive} sizeof(Tbl),(sizeof(Tbl)*2)-1\n',[2*width,4*width-1])):
                expected=b''.join(n.to_bytes(width,'little') for n in numbers).hex()
                for wrapper in ('%s','.macro Emit\n%s.endm\nEmit\n',
                                '.proc P\n%s.endp\n','.rept 1\n%s.endm\n'):
                    for debug in (False,True):
                        with self.subTest(directive=directive,body=body,wrapper=wrapper,debug=debug):
                            self.assemble(wrapper%body,expected,options=('--debug',) if debug else ())
        self.assemble('Tbl:.db sizeof(Tbl)\nNext:.db sizeof(Tbl)\n','01 01')
        self.assemble('Other:.db 1,2\nTbl:.db sizeof(Tbl)\n.db sizeof(Other)\n','01 02 02 02')
        self.assemble('.macro Emit\nTbl:.db sizeof(Tbl),sizeof(Tbl)\n.undef Tbl\n.endm\nEmit\nEmit\n',
                      '02 02 02 02')

    def test_repeated_sizeof_preserves_string_lengths_and_kind_guards(self):
        # The approved known-at-use string rule counts AB as two characters
        # even before that data statement captures it; v1's first size was 3.
        self.assemble('S="AB"\nTbl:.db sizeof(Tbl)\n.db S,sizeof(Tbl)\nS=7\n','04 41 42 04')
        self.assemble('S="AB"\nTbl:.dw sizeof(Tbl)\n.dw S,sizeof(Tbl)\nS=7\n',
                      '08 00 41 00 42 00 08 00')
        for operands in ('sizeof(Tbl),sizeof(Tbl)+"A"',
                         'sizeof(Tbl)+"A",sizeof(Tbl)',
                         'sizeof(Tbl),sizeof(Tbl)+S',
                         'sizeof(Tbl)+S,sizeof(Tbl)'):
            for debug in (False,True):
                with self.subTest(operands=operands,debug=debug):
                    r=self.assemble(f'Tbl:.db {operands}\nS .equ "AB"\n',
                                    error="size of `Tbl' is unknown",
                                    options=('--debug',) if debug else ())
                    self.assertIn('in.asm:1: error:',r.stderr)
        self.assemble('Tbl:.db sizeof(Tbl),S,sizeof(Tbl)\nS .equ "AB"\n',
                      error="size of `Tbl' is unknown")

    def test_sizeof_labels_sharing_data_do_not_form_a_false_cycle(self):
        for labels in (('A1','Tbl'),('Tbl','A1'),('One','Two','Three','Tbl')):
            for directive,width in (('.db',1),('.dw',2),('.dd',4)):
                for reverse in (False,True):
                    operands=list(reversed(labels)) if reverse else labels
                    size=len(labels)*width
                    body=''.join(name+':\n' for name in labels)+directive+' '+','.join(
                        f'sizeof({name})' for name in operands)+'\n'
                    expected=(size.to_bytes(width,'little')*len(labels)).hex()
                    for wrapper in ('%s','.macro Emit\n%s.endm\nEmit\n',
                                    '.proc P\n%s.endp\n','.rept 1\n%s.endm\n'):
                        for debug in (False,True):
                            with self.subTest(labels=labels,directive=directive,reverse=reverse,
                                              wrapper=wrapper,debug=debug):
                                self.assemble(wrapper%body,expected,options=('--debug',) if debug else ())
        self.assemble('.macro Emit\nTbl:.db sizeof(A1),sizeof(Tbl)\n.endm\nA1:\nEmit\n','02 02')
        self.assemble('A1:\nTbl:.db sizeof(A1)\n.db sizeof(Tbl)+1\n','02 03')

    def test_sizeof_shared_labels_skip_exitm_tombstones(self):
        # .exitm removes the remaining body but leaves internal tombstones
        # between A1 and Tbl. They must not split the shared data scan.
        cases=[('.macro M\nA1:\n.exitm\n.db 99\n.endm\nM\n'
                'Tbl:.db sizeof(A1),sizeof(Tbl)\n','02 02'),
               ('.macro M\nA1:\n.exitm\n.db 99\n.db 98\n.endm\nM\n'
                'Tbl:.dw sizeof(A1)\n.db sizeof(Tbl)\n','03 00 03'),
               ('.macro M v\nA1:\n.if v\n.exitm\n.endif\n.db 99\n.endm\nM 1\n'
                'Tbl:.db sizeof(A1),sizeof(Tbl)\n','02 02')]
        for source,expected in cases:
            for debug in (False,True):
                with self.subTest(source=source,debug=debug):
                    self.assemble(source,expected,options=('--debug',) if debug else ())

    def test_sizeof_unrelated_labels_still_resolves_string_values(self):
        # Matching any label's sizeof, rather than the current data run,
        # would classify the final operand as an unknown integer and reject.
        self.assemble('Other:.db 1,2\nTbl:.db sizeof(Tbl)\n.db sizeof(Other)+"A"\n',
                      '01 02 03 32 41')
        self.assemble('Other:.db 1,2\nA1:\nTbl:.db sizeof(A1),sizeof(Tbl),sizeof(Other)+"A"\n',
                      '01 02 04 04 32 41')
        self.assemble('A1:\n.codeseg\nTbl:.db sizeof(A1),sizeof(Tbl)\n',
                      error="size of `A1' is unknown")
        self.assemble('A1:\nTbl:.db sizeof(A1)\n.dsb sizeof(Tbl)\n',
                      error='circular size definition')

    def test_self_sized_strings_are_rejected_by_owner_policy(self):
        cases=[('Tbl:.db sizeof(Tbl)+"A"\n',1),
               ('Tbl:.db 1,((sizeof(Tbl)))+"A"\n',1),
               ('S="AB"\nTbl:.db S+sizeof(Tbl)\n',2),
               ('S="AB"\nTbl:.db sizeof(Tbl),S+sizeof(Tbl)\n',2)]
        for source,line in cases:
            for debug in (False,True):
                with self.subTest(source=source,debug=debug):
                    r=self.assemble(source,error="size of `Tbl' is unknown",
                                    options=('--debug',) if debug else ())
                    self.assertIn(f'in.asm:{line}: error:',r.stderr)
        self.assemble('Tbl:.dw <(sizeof(Tbl))\n','02 00')
        self.assemble('K=3\nTbl:.db sizeof(Tbl),sizeof(Tbl)*K\n','02 06')
        self.assemble('S=5\nTbl:.db sizeof(Tbl),S+sizeof(Tbl)\nS="AB"\n','02 07')

    def test_repeated_sizeof_does_not_hide_real_size_cycles_or_operand_errors(self):
        for source,line,error in (
                ('Tbl:.db sizeof(Tbl)\n.dsb sizeof(Tbl)\n',2,'circular size definition'),
                ('.struc S\nf .byte[sizeof S]\n.ends\nv .S\n',2,'circular size definition'),
                ('Tbl:.db sizeof(Tbl),sizeof(Tbl)\n.dsb N\nN=2\n',1,"size of `Tbl' is unknown"),
                ('Tbl:.db sizeof(Tbl)\n.db sizeof(Tbl)/0\n',2,'division by zero'),
                ('Tbl:.db sizeof(Tbl)\n.db sizeof(Nope)\n',2,"size of `Nope' is unknown")):
            with self.subTest(source=source):
                r=self.assemble(source,error=error)
                self.assertIn(f'in.asm:{line}: error:',r.stderr)

    def test_sizeof_anonymous_guards_keep_expansion_scope(self):
        cases=[('Tbl:.db sizeof(Tbl)\n.dw +\n+ nop\n.undef Tbl\n','03 03 00 EA 03 07 00 EA'),
               ('- nop\nTbl:.db sizeof(Tbl)\n.dw -\n.undef Tbl\n','EA 03 00 00 EA 03 04 00')]
        for body,expected in cases:
            for wrapper in ('.macro Emit\n%s.endm\nEmit\nEmit\n','.rept 2\n%s.endm\n',
                            'I=0\n.while I<2\n%sI=I+1\n.endm\n'):
                for debug in (False,True):
                    with self.subTest(body=body,wrapper=wrapper,debug=debug):
                        self.assemble(wrapper%body,expected,options=('--debug',) if debug else ())

    def test_sizeof_keeps_guards_for_uncaptured_same_statement_operands(self):
        self.assemble('Tbl:.db sizeof(Tbl),S\nS=7\n','02 07')
        self.assemble('Tbl:.dw sizeof(Tbl),L\nL:nop\n','04 00 04 00 EA')
        for operand,alias in (('S',''),('A1','A1 .equ S\n')):
            for text in ('','A','ABCD'):
                source=alias+f'Tbl:.db sizeof(Tbl),{operand}\nS .equ "{text}"\n'
                with self.subTest(operand=operand,text=text):
                    r=self.assemble(source,error="size of `Tbl' is unknown")
                    self.assertIn(f'in.asm:{1+alias.count(chr(10))}: error:',r.stderr)

    def test_sizeof_sees_later_local_operands_in_the_same_statement(self):
        for directive,expected in (('.db','02 02 EA'),('.dw','04 00 04 00 EA')):
            body=f'Tbl: {directive} sizeof(Tbl),@@y\n@@y:nop\n'
            for macro in (False,True):
                source='.macro Emit\n'+body+'.endm\nEmit\n' if macro else body
                for debug in (False,True):
                    with self.subTest(directive=directive,macro=macro,debug=debug):
                        self.assemble(source,expected,options=('--debug',) if debug else ())
        # Forward-name widths before a source-time PC remain conservative.
        self.assemble('foo:nop\n.dw @@f\n.if $==3\n.db $33\n.endif\n@@f:nop\n',
                      error='cannot use $ before final layout')

    def test_pc_width_checks_cover_the_whole_selected_prefix(self):
        source='L:nop\nM:nop\n.dw L\n.dw M\n.if $==6\n.db $33\n.endif\n'
        self.assemble(source,'EA EA 00 00 01 00 33')
        r=self.assemble(source+'.undef L\nL .equ "AB"\n',error='cannot use $ before final layout')
        self.assertIn('in.asm:5: error:',r.stderr)

    def test_pc_width_checks_keep_both_segment_prefixes(self):
        # Both segments have recorded operands. A code origin must neither
        # select nor discard the RAM assumptions used by the later capture.
        for reset in ('','.org $8000\n'):
            source=('.dataseg\nL:.byte 0\n.dw L\n.codeseg\nM:nop\n.dw M\n'+reset+
                    '.dataseg\nHere .equ $\n.codeseg\n.dw Here\n')
            self.assemble(source,'EA 00 00 03 00')
            r=self.assemble(source+'.undef L\nL .equ "AB"\n',error='cannot use $ before final layout')
            line=source.splitlines().index('Here .equ $')+1
            self.assertIn(f'in.asm:{line}: error:',r.stderr)

    def test_pc_width_guards_preserve_numeric_aliases_and_origin_segment_boundaries(self):
        for alias,value in (('L+1','01 00'),('L','00 00')):
            self.assemble(f'L:nop\nA1 .equ {alias}\n.dw A1\n.if $ == 3\n.db $33\n.endif\n',
                          'EA '+value+'33')
        self.assemble('L:nop\n.dw L\n.if $==3\n.db $33\n.endif\n.undef L\nL:rts\n','EA 04 00 33 60')
        self.assemble('L:nop\n.dw L\n.if $==3\n.db $33\n.endif\n.undef L\nL .equ 7\n','EA 07 00 33')
        # Changed-kind data needs no rejection if no source-time size/PC used it.
        self.assemble('L:nop\n.dw L\n.undef L\nL .equ "ABCD"\n','EA 41 00 42 00 43 00 44 00')
        self.assemble('L:nop\n.dw L\n.org $8000\n.if $==$8000\n.db $33\n.endif\n.undef L\nL .equ "ABCD"\n',
                      'EA 41 00 42 00 43 00 44 00 33')
        self.assemble('.dataseg\nL:.byte 0\n.dw L\n.codeseg\n.if $==0\n.db $33\n.endif\n.undef L\nL .equ "ABCD"\n','33')
        # An origin cannot erase an earlier PC use's obligation.
        self.assemble('L:nop\n.dw L\nHere .equ $\n.org $8000\n.dw Here\n.undef L\nL .equ "ABCD"\n',
                      error='cannot use $ before final layout')
        self.assemble('.dataseg\nL:.byte 0\n.dw L\n.codeseg\nnop\n.dataseg\nHere .equ $\n.codeseg\n.dw Here\n.undef L\nL .equ "ABCD"\n',
                      error='cannot use $ before final layout')
        self.assemble('L:nop\n.dw L\n.codeseg\nHere .equ $\n.undef L\nL .equ "ABCD"\n',
                      error='cannot use $ before final layout')

    def test_pc_width_guard_operands_survive_expansion_until_validation(self):
        body='.dw L\n.if $==3\n.db $33\n.endif\n'
        wrappers=('.macro Emit\n%s.endm\nEmit\n','.rept 1\n%s.endm\n','.proc Emit\n%s.endp\n')
        for wrapper in wrappers:
            for debug in (False,True):
                with self.subTest(wrapper=wrapper,debug=debug):
                    self.assemble('L:nop\n'+wrapper%body+'.undef L\nL .equ "ABCD"\n',
                                  error='cannot use $ before final layout',options=('--debug',) if debug else ())

    def test_nested_macro_local_binding_and_explicit_label_marker(self):
        for debug in (False,True):
            options=('--debug',) if debug else ()
            for inner,expected in (('@@x: nop\n','EA EA A5 02'),('',None)):
                source=('.macro M2 q\n'+inner+'lda q\n.endm\n.macro M1 p\nM2 p+1\n.endm\n'
                        'Start:\n@@x: nop\nM1 @@x\n')
                with self.subTest(inner=inner,debug=debug):
                    self.assemble(source,expected,error=None if expected else 'undefined local label',options=options)
            for wrapper in ('.macro Emit\n%s\n.endm\nEmit\n','.rept 2\n%s\n.endm\n'):
                with self.subTest(wrapper=wrapper,debug=debug):
                    self.assemble(wrapper % '.label fx = $C000',error='explicit-address .label',options=options)

    def test_branch_plus_128_has_located_error_and_preserves_output(self):
        for debug in (False,True):
            options=('--debug',) if debug else ()
            self.assemble('bne Target\n.dsb 127\nTarget:nop\n','D0 7F'+'00'*127+'EA',options=options)
            r=self.assemble('bne Target\n.dsb 128\nTarget:nop\n',error='operand out of range',options=options)
            self.assertIn('in.asm:1: error:',r.stderr)

    def test_source_time_pc_corrections_and_deferred_width_rejections(self):
        # v1 captured a provisional 3 here; D0/D1 require the known final PC, 2.
        for declaration in ('Here .equ $','Here = $','.define Here $'):
            self.assemble('Z .equ $10\nlda Z\n'+declaration+'\n.dw Here\n','A5 10 02 00')
            for prefix,suffix in (('lda Fwd\n','Fwd .equ $10\n'),('.db S\n','S .equ "AB"\n'),
                                  ('.dsb N\n','N .equ 2\n')):
                with self.subTest(declaration=declaration,prefix=prefix):
                    self.assemble(prefix+declaration+'\n.dw Here\n'+suffix,error='cannot use $')
        for prefix,suffix in (('.db S\n','S .equ "AB"\n'),('.dsb N\n','N .equ 2\n')):
            with self.subTest(prefix=prefix):
                self.assemble(prefix+'.if $==1\n.db 17\n.endif\n'+suffix,error='cannot use $ before final layout')

    def test_ram_data_keeps_source_time_truncation_warnings(self):
        for directive,value in (('.byte','300'),('.word','70000'),('.byte','Fwd')):
            source=f'.dataseg\n.org $10\nt1 {directive} {value}\n.codeseg\nFwd .equ 300\nnop\n'
            with self.subTest(directive=directive,value=value):
                r=self.assemble(source,'EA')
                self.assertEqual(r.stderr,'in.asm:3: warning: operand out of range; truncated\n')
        # Mixed data takes the fixup path, not the fully literal compaction path.
        r=self.assemble('.dataseg\n.org $10\nt1 .byte 300,<Target\n.codeseg\nTarget:nop\n','EA')
        self.assertEqual(r.stderr,'in.asm:3: warning: operand out of range; truncated\n')
        # v1 does not perform late truncation diagnostics for uninitialized RAM.
        r=self.assemble('.dataseg\n.org $10\nt1 .byte Target+300\n.codeseg\nTarget:nop\n','EA')
        self.assertEqual(r.stderr,'')
        r=self.assemble('.dataseg\n.org $10\nt1 .char 300\n.codeseg\nnop\n','EA')
        self.assertEqual(r.stderr,'')

    def test_expansion_pc_type_pc_and_explicit_label_rejections(self):
        self.assemble('lda Fwd\n.if $ == 3\n.db 3\n.endif\nFwd .equ $10\n',error='cannot use $ before final layout')
        self.assemble('nop\n.if $ == 1\n.db 1\n.endif\n','EA 01')
        for body in ('.struc S\nf .byte[$+1]\n.ends\n','.union U\nf .byte[$]\n.ends\n'):
            self.assemble(body,error='$ is not allowed in type field dimensions')
        self.assemble('nop\nP .equ $+1\n.struc S\nf .byte[P]\n.ends\nv .S\n','EA 00 00')
        for value in ('$C000','$'):
            self.assemble(f'.label fx = {value}\njmp fx\n',error='explicit-address .label')
        self.assemble('.label fx\njmp fx\n','4C 00 00')

    def test_sizeof_source_time_scan_and_kind_checks(self):
        cases=[('.dataseg\np1:\np2:\n.dsb 2\np3 .byte\n.codeseg\nlda #sizeof(p1)\n','A9 02'),
               ('.dataseg\nbuf:\n.dsb 2\n.codeseg\nlda #sizeof(buf)\n.dataseg\n.dsb 3\nnxt .byte\n','A9 02'),
               ('Tbl:.db 1,2\nK .equ 5\n.db 3\n.byte sizeof(Tbl)\n','01 02 03 04'),
               ('Tbl:.db 1,2\n.public Tbl\n.db 3\n.byte sizeof(Tbl)\n','01 02 03 02'),
               ('Msg:.char S\nS .equ "HELLO"\n.byte sizeof(Msg)\n','48 45 4C 4C 4F 06'),
               ('Msg:.byte V\n.byte sizeof(Msg)\nV .equ 7\n','07 02'),
               ('Tbl:.db 1,2\n.db (sizeof(Tbl)+1)*2\n','01 02 08'),
               ('Tbl:.dw H1,H2\n.byte sizeof(Tbl)\nH1:rts\nH2:rts\n','05 00 06 00 05 60 60')]
        for source,expected in cases:
            with self.subTest(source=source):self.assemble(source,expected)
        for source in ('.byte sizeof(Tbl)\nTbl:.db 1,2,3\n','Buf:.dsb N\n.byte sizeof(Buf)\nN .equ 3\n',
                       'Msg:.char S\n.byte sizeof(Msg)\nS .equ "HELLO"\n',
                       '.extrn Ext:byte\nK .equ Ext\nTbl:.db K\n.byte sizeof(Tbl)\n'):
            with self.subTest(source=source):self.assemble(source,error='is unknown')

    def test_d1_sizeof_scan_matrix(self):
        cases=json.loads((REPO/'tests/v2_sizeof_cases.json').read_text())['cases']
        for case in cases:
            for name, content in case.get('files',{}).items():
                (self.directory/name).write_text(content)
            for name, content in case.get('hex_files',{}).items():
                (self.directory/name).write_bytes(bytes.fromhex(content))
            for debug in (False,True):
                with self.subTest(case=case['id'],debug=debug):
                    self.assemble(case['source'],case.get('bytes'),case.get('error'),
                                  options=('--debug',) if debug else ())

    def test_character_maps_are_captured_and_strings_keep_lengths(self):
        (self.directory/'first.map').write_text('H=1\nI=0\n')
        (self.directory/'second.map').write_text('H=2\nI=3\n')
        self.assemble('.charmap "first.map"\n.char S,"HI",72\n.charmap "second.map"\n.char S\nS .equ "HI"\n',
                      '01 00 01 00 48 02 03')
        self.assemble('.db 12+"N","N"+$FFFFFFFF,"B"<"a","ab"<"abc"\n','31 32 4E 4E 2D 31 01 01')
        self.assemble('.word S,$\nS .equ ""\n','00 00')

    def test_forward_mapped_initializer_uses_its_final_length_in_binary(self):
        (self.directory/'map').write_text('H=1\nI=0\n')
        source='.charmap "map"\n.struc S\nf .char[2]\ng .byte\n.ends\nv .S {Text,7}\nText .equ "HI"\n'
        for options in ((), ('--debug',)):
            with self.subTest(options=options):
                self.assemble(source, '01 00 07', options=options)

    def test_alias_graphs_share_dependencies_and_detect_cycles(self):
        source='.word C0\n'+''.join(f'C{i} .equ C{i+1}\n' for i in range(10000))+'C10000 .equ 42\n'
        self.assemble(source,'2A 00')
        source='.word C0\n'+''.join(f'C{i} .equ C{i+1}+C{i+1}\n' for i in range(200))+'C200 .equ Tail-Head\nHead:nop\nTail:rts\n'
        self.assemble(source,'00 00 EA 60')
        self.assemble('AA .equ BB\nBB .equ AA\n.word AA\n',error='circular')
        self.assemble('.byte Alias\nAlias .equ Value\nValue=3\n.byte Alias\nValue=4\n.byte Alias\n','04 03 04')

    def test_forward_structural_aliases_keep_captured_definitions(self):
        point='.struc Point\npx .byte\npy .byte\n.ends\n'
        aliases='Total .equ Off+1\nOff .equ Point::py\n'
        cases=[(aliases+point+'.db Total\n','02'),
               (aliases+'.db Total\n'+point,'02'),
               (aliases+'.db Total\n'+point+'.if Total==2\n.db $AA\n.endif\n','02 AA'),
               ('Total .equ Off+1\nOff = Point::py\n.db Total\nOff = 4\n'+point+'.db Total\n','02 05'),
               ('Total .equ Off+1\n.define Off Point::py\n.db Total\n.undef Off\n.define Off 4\n'+point+'.db Total\n','02 05'),
               ('Total .equ Bits+1\nBits .equ mask R::lo\n.db Total\n.record R hi:3,lo:5\n','20'),
               ('.proc Main\n'+aliases+'.db Total\n'+point+'.endp\n','02')]
        for source,expected in cases:
            for debug in (False,True):
                with self.subTest(source=source,debug=debug):
                    self.assemble(source,expected,options=('--debug',) if debug else ())
        self.assemble('Total .equ Off+1\nOff .equ Point::missing\n.db Total\n'+point,
                      error='expression does not evaluate to literal')
        self.assemble(aliases+'.db Total\n.struc Point\npx .byte[Total]\npy .byte\n.ends\n',
                      error='circular')

    def test_string_limit_is_checked_during_frontend_folding(self):
        source='S0 .equ "A"\n'+''.join(f'S{i} .equ S{i-1}+S{i-1}\n' for i in range(1,26))+'.db S25\n'
        self.assemble(source,error='string resource limit exceeded')

    def test_ram_data_values_keep_fixed_extents_and_validate_after_layout(self):
        self.assemble('.dataseg\nTbl:.dw TargetAddr\nNext:.db 0\n.codeseg\nTargetAddr:.dw Next\n','02 00')
        for operand in ('"A"+TargetAddr','Alias'):
            source=f'.dataseg\n.db {operand}\n.codeseg\nTargetAddr:nop\nAlias .equ "A"+TargetAddr\n'
            with self.subTest(operand=operand):
                self.assemble(source,error='string value depends on placement')
        self.assemble('.dataseg\n.db TargetAddr/0\n.codeseg\nTargetAddr:nop\n',error='division by zero')

    def test_member_tokens_remain_distinct_from_constants(self):
        self.assemble('.struc P\npx .byte\npy .byte\n.ends\npy=5\narr .P[3]\nAlias .equ arr\nlda Alias[1].py\nlda arr[py-py+2].px\n',
                      '00 00 00 00 00 00 A5 03 A5 04')

    def test_fixed_width_data_allows_expansion_pc(self):
        self.assemble('Lab: .db <Fwd\n.if $==1\n.db 1\n.endif\nFwd:rts\n','02 01 60')
        self.assemble('Lab: .dw Lab\n.if $==2\n.db 1\n.endif\n','00 00 01')
        self.assemble('lda $10\n.if $==2\n.db 1\n.endif\n','A5 10 01')

    def test_sizeof_macro_exit_labels_segments_and_maps(self):
        bodies=[('.macro Emit\n.db 3\n.exitm\n.db 9\n.endm\nEmit\n', '01 02 03 04'),
                ('.macro Emit\nStop:\n.db 3\n.endm\nEmit\n','01 02 03 02'),
                ('.codeseg\n.db 3\n','01 02 03 02')]
        for middle,expected in bodies:
            with self.subTest(middle=middle):self.assemble('Tbl:.db 1,2\n'+middle+'.db sizeof(Tbl)\n',expected)
        (self.directory/'map.txt').write_text('A=2\n')
        self.assemble('Tbl:.db 1,2\n.charmap "map.txt"\n.db 3,sizeof(Tbl)\n','01 02 03 02')

    def test_includes_and_binary_data(self):
        (self.directory/'inc.asm').write_text('.db 1,2\n')
        (self.directory/'blob').write_bytes(bytes([3,0,4]))
        self.assemble('.include "inc.asm"\n.incbin "blob"\n.db $\n','01 02 03 00 04 05')

    def listed(self, source, expected, debug=False):
        formats = []
        for fmt in ('json', 'ndjson'):
            options = ('--listing=rows', '--listing-format='+fmt, *(['--debug'] if debug else []))
            self.assemble(source, expected, options=options)
            text = (self.directory/'rows').read_text()
            rows = json.loads(text)['records'] if fmt == 'json' else [json.loads(x) for x in text.splitlines()]
            emitted = bytearray()
            for row in rows:
                self.assertEqual(row['version'], '2')
                count = row['byte_count']
                address = int(row['cpu_address_start'], 16)
                self.assertEqual(int(row['cpu_address_end'], 16), (address + max(0, count-1)) & 65535)
                if row['emits_bytes']:
                    self.assertEqual(row['output_offset_start'], len(emitted))
                    self.assertEqual(row['output_offset_end'], len(emitted)+count-1)
                    data = bytes(count) if row['fill_byte'] == '00' else bytes.fromhex(''.join(row['bytes_hex']))
                    self.assertEqual(len(data), count)
                    emitted += data
                else:
                    self.assertIsNone(row['output_offset_start'])
                    self.assertIsNone(row['output_offset_end'])
                    self.assertEqual(row['bytes_hex'], [])
            self.assertEqual(bytes(emitted), bytes.fromhex(expected))
            formats.append(rows)
        self.assertEqual(*formats)
        return formats[0]

    def test_listing_rows_use_final_opcodes_offsets_ram_and_fill_extents(self):
        source = ('.org $8000\nStart: lda Target\nasl\nbne Start\n.db 1,2,3,4,5\n.dsw 3\n'
                  '.dataseg\n.org $10\nTarget: .byte 1,2\n.dsw 2\n.codeseg\nEnd: nop\n')
        for debug in (False, True):
            rows = self.listed(source, 'A5 10 0A D0 FB 01 02 03 04 05 00 00 00 00 00 00 EA', debug)
            instructions = [r for r in rows if r['addressing_mode']]
            self.assertEqual([r['addressing_mode'] for r in instructions],
                             ['zeropage', 'accumulator', 'relative', 'implied'])
            self.assertEqual([r['output_offset_start'] for r in instructions], [0,2,3,16])
            ram = [r for r in rows if r['segment']=='data']
            self.assertEqual([(r['cpu_address_start'], r['byte_count']) for r in ram],
                             [('0x0000',0),('0x0010',0),('0x0010',0),('0x0010',2),('0x0012',4)])
            fills = [r for r in rows if r['fill_byte']]
            self.assertEqual([(r['byte_count'],r['output_offset_start']) for r in fills],[(6,10)])

    def test_listing_deferred_string_maps_and_layout_cascade_match_emitted_bytes(self):
        (self.directory/'map.txt').write_text('H=1\nI=0\n')
        cases = [('.charmap "map.txt"\n.struc Text\ns .char[2]\nn .byte\n.ends\n'
                  'v .Text { S, 7 }\nS .equ "HI"\nEnd: nop\n', '01 00 07 EA'),
                 ('.org $F6\nlda L\nlda M\nM: nop\nnop\nnop\nnop\nL: rts\n',
                  'A5 FE A5 FA EA EA EA EA 60'),
                 ('.org $FFFE\n.db 1,2,3,4,5\nAfter: nop\n', '01 02 03 04 05 EA'),
                 ('.db ""\n.dword "AB"\n.dw Fwd\nFwd: nop\n', '41 00 00 00 42 00 00 00 0A 00 EA'),
                 ('K .equ 1\n', '')]
        for source, expected in cases:
            for debug in (False, True):
                with self.subTest(source=source, debug=debug): self.listed(source, expected, debug)

    def test_listing_keeps_source_boundaries_and_non_debug_data_grouping(self):
        source = '.db 1,2\n.db 3,4,5\nOther: .db 6\n'
        for debug in (False,True):
            rows = self.listed(source, '01 02 03 04 05 06', debug)
            data = [r for r in rows if r['byte_count']]
            self.assertEqual([r['line'] for r in data], [1,2,3] if debug else [1,1,3])
            self.assertEqual([r['continuation_of_record'] for r in data],
                             [False,False,False] if debug else [False,True,False])
            fills = self.listed('.dsb 1\n.dsw 2\n', '00 00 00 00 00', debug)
            self.assertEqual(len(fills), 1)
            self.assertEqual(fills[0]['byte_count'], 5)
        (self.directory/'inc.asm').write_text('.db 1,2\n')
        (self.directory/'blob').write_bytes(bytes.fromhex('03 00 04'))
        rows = self.listed('.include "inc.asm"\n.incbin "blob"\n', '01 02 03 00 04')
        data = [r for r in rows if r['emits_bytes']]
        self.assertEqual(Path(data[0]['file']).name, 'inc.asm')
        self.assertEqual(data[-1]['directive_or_opcode'], 'INCBIN')

    def test_listing_failures_keep_previous_outputs_and_reject_other_modes(self):
        source = '.byte 300\n.word 70000\n.dataseg\nR .byte 300\n.codeseg\n'
        baseline = self.assemble(source, '2C 70 11').stderr
        for fmt in ('json','ndjson'):
            actual = self.assemble(source, '2C 70 11',
                                   options=('--listing=rows','--listing-format='+fmt)).stderr
            self.assertEqual(actual, baseline)
        for source in ('lda Missing\n', 'bne Target\n.dsb 128\nTarget: nop\n'):
            (self.directory/'rows').write_text('previous listing')
            r=self.assemble(source, error='error:', options=('--listing=rows','--listing-format=json'))
            self.assertEqual((self.directory/'rows').read_text(),'previous listing')
            self.assertNotIn('no error',r.stderr)
        (self.directory/'rows').write_text('previous listing')
        self.assemble('.db 1\n', error='I5 prototype listings require',
                      options=('--listing=rows','--listing-format=json','--listing-version=1'))
        self.assertEqual((self.directory/'rows').read_text(),'previous listing')
        self.assemble('.db 1\n', error='aliases', error_code=3,
                      options=('--listing=in.asm','--listing-format=json'))
        self.assertEqual((self.directory/'in.asm').read_text(),'.db 1\n')

    def test_output_aliases_and_unsupported_modes_preserve_files(self):
        self.assemble('.db 1\n',error='I5 prototype listings require',options=('--listing=kept.lst',))
        (self.directory/'inc.asm').write_text('.db 2\n')
        self.assemble('.include "inc.asm"\n',error='aliases',options=('-o','inc.asm'),error_code=3)
        self.assertEqual((self.directory/'inc.asm').read_text(),'.db 2\n')

if __name__=='__main__':
    unittest.main()
