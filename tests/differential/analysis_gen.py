"""Generated analysis cases, adapted from the preserved 2.0 reviewer generator.
gen(rng) -> (main_source, {aux_name: bytes})"""
import random

MODES_ZP = ['lda {z}', 'sta {z}', 'ldx {z}', 'inc {z}', 'lda {z},x', 'ldy {z},x', 'lda [{z}],y', 'lda [{z},x]', 'stx {z},y']
MODES_ABS = ['lda {a}', 'sta {a}', 'lda {a},x', 'lda {a},y', 'jmp {a}', 'jsr {a}', 'inc {a}', 'bit {a}', 'jmp [{a}]', 'sta {a},y']
IMPLIED = ['nop', 'inx', 'dey', 'clc', 'sec', 'rts', 'tax', 'pha', 'pla']
ACC = ['asl', 'asl a', 'lsr', 'rol a', 'ror', 'lsr a']
BR = ['bne', 'beq', 'bcc', 'bcs', 'bpl', 'bmi']


class G:
    def __init__(self, rng):
        self.r = rng
        self.lines = []
        self.labels = []        # code labels defined so far (global)
        self.fwd = []           # code labels to define later
        self.ram = []           # ram labels
        self.zp = []            # zero-page ram labels
        self.consts = {}        # name -> kind ('int','addr','str')
        self.n = 0
        self.locals = []        # local labels in current scope
        self.procs = []
        self.types = []         # (name, [(field, size)])
        self.anon_pending = 0
        self.charmap = False
        self.reassign = []

    def name(self, p):
        self.n += 1
        return f'{p}{self.n}'

    def emit(self, s):
        self.lines.append(s)

    def addr_expr(self):
        r = self.r
        pool = self.labels + self.fwd + self.ram
        c = r.random()
        if pool and c < 0.55:
            base = r.choice(pool)
            k = r.random()
            if k < 0.6: return base
            if k < 0.8: return f'{base}+{r.randrange(1, 5)}'
            return f'{base}-{r.randrange(1, 3)}'
        addrs = [n for n, k in self.consts.items() if k == 'addr']
        if addrs and c < 0.8: return r.choice(addrs)
        if c < 0.9: return f'${r.randrange(0x200, 0x7ff):04X}'
        return '$'

    def zp_expr(self):
        r = self.r
        if self.zp and r.random() < 0.6:
            z = r.choice(self.zp)
            return z if r.random() < 0.7 else f'{z}+1'
        ints = [n for n, k in self.consts.items() if k == 'zp']
        if ints and r.random() < 0.5: return r.choice(ints)
        return f'${r.randrange(0, 0xfe):02X}'

    def imm(self):
        r = self.r
        c = r.random()
        ints = [n for n, k in self.consts.items() if k == 'int']
        pool = self.labels + self.fwd + self.ram
        if ints and c < 0.3: return f'#{r.choice(ints)}'
        if pool and c < 0.55: return f'#<{r.choice(pool)}' if r.random() < 0.5 else f'#>{r.choice(pool)}'
        if c < 0.65: return '#<$'
        if self.types and c < 0.75:
            t, fields = r.choice(self.types)
            return f'#{t}::{r.choice(fields)[0]}'
        if c < 0.8: return f'#sizeof({r.choice(pool)})' if pool and self.ram else '#1'
        return f'#{r.randrange(0, 256)}'

    def instr(self):
        r = self.r
        c = r.random()
        if c < 0.12: self.emit(r.choice(IMPLIED))
        elif c < 0.2: self.emit(r.choice(ACC))
        elif c < 0.35: self.emit(r.choice(['lda', 'ldx', 'ldy', 'cmp', 'and', 'adc']) + ' ' + self.imm())
        elif c < 0.5: self.emit(r.choice(MODES_ZP).format(z=self.zp_expr()))
        elif c < 0.68: self.emit(r.choice(MODES_ABS).format(a=self.addr_expr()))
        elif c < 0.8:
            op = r.choice(BR)
            k = r.random()
            if k < 0.25 and self.locals: self.emit(f'{op} {r.choice(self.locals)}')
            elif k < 0.45: self.emit(f'{op} -') if self.anon_back else self.emit(f'{op} +'); self.anon_pending += (not self.anon_back) * 1
            elif k < 0.6: self.emit(f'{op} +'); self.anon_pending += 1
            elif self.labels and k < 0.85: self.emit(f'{op} {self.labels[-1]}')
            else:
                l = self.name('F'); self.fwd.append(l); self.emit(f'{op} {l}'); self.close_soon.append(l)
        elif c < 0.9 and self.consts:
            n = r.choice(list(self.consts))
            k = self.consts[n]
            if k == 'int': self.emit(f'ldx #{n}')
            elif k == 'zp': self.emit(f'lda {n}')
            elif k == 'addr': self.emit(f'lda {n},y')
            elif k == 'str': self.emit(f'.db {n}')
        elif self.types and self.ram_inst:
            v, t, fields = r.choice(self.ram_inst)
            self.emit(f'lda {v}.{r.choice(fields)[0]}')
        else:
            self.emit('lda ' + self.addr_expr() + ',x')

    def data(self):
        r = self.r
        kind = r.choice(['.db', '.db', '.dw', '.dd', '.char', '.byte', '.word'])
        items = []
        for _ in range(r.randrange(1, 7)):
            c = r.random()
            pool = self.labels + self.fwd
            if c < 0.35: items.append(str(r.randrange(0, 256 if kind in ('.db', '.byte', '.char') else 65536)))
            elif c < 0.5 and kind in ('.db', '.char', '.byte', '.dd'): items.append('"' + r.choice(['A', 'HI', 'XYZ', 'ab', '']) + '"')
            elif c < 0.65 and pool: items.append(('<' if kind in ('.db', '.byte', '.char') else '') + r.choice(pool))
            elif c < 0.72 and pool and kind in ('.db', '.byte'): items.append('>' + r.choice(pool))
            elif c < 0.8: items.append('$' if kind not in ('.db', '.byte', '.char') else '<$')
            elif c < 0.9:
                strs = [n for n, k in self.consts.items() if k == 'str']
                ints = [n for n, k in self.consts.items() if k in ('int', 'zp')]
                if strs and kind in ('.db', '.char', '.byte'): items.append(r.choice(strs))
                elif ints: items.append(r.choice(ints))
                else: items.append('7')
            else:
                items.append(f'{r.randrange(1, 9)}*{r.randrange(1, 9)}')
        self.emit(f'{kind} ' + ', '.join(items))
        if r.random() < 0.4:  # data grouping: consecutive same-type rows
            self.emit(f'{kind} {r.randrange(0, 200)}, {r.randrange(0, 200)}')

    def label(self):
        r = self.r
        if self.close_soon and r.random() < 0.6:
            l = self.close_soon.pop(0)
            self.fwd.remove(l)
        elif self.fwd and r.random() < 0.3:
            l = r.choice(self.fwd); self.fwd.remove(l)
            if l in self.close_soon: self.close_soon.remove(l)
        else:
            l = self.name('L')
        self.emit(f'{l}:')
        self.labels.append(l)
        self.locals = []

    def const(self):
        r = self.r
        c = r.random()
        n = self.name('K')
        if c < 0.25: self.emit(f'{n} .equ {r.randrange(0, 256)}'); self.consts[n] = 'int'
        elif c < 0.35: self.emit(f'{n} = ${r.randrange(0, 0xfe):02X}'); self.consts[n] = 'zp'
        elif c < 0.6 and (self.labels or self.fwd or self.ram):
            base = r.choice(self.labels + self.fwd + self.ram)
            self.emit(f'{n} .equ {base}+{r.randrange(0, 4)}'); self.consts[n] = 'addr'
        elif c < 0.7 and self.consts:
            prev = [k for k, v in self.consts.items() if v in ('addr',)]
            if prev:
                self.emit(f'{n} .equ {r.choice(prev)}+1'); self.consts[n] = 'addr'
            else:
                self.emit(f'{n} = 3'); self.consts[n] = 'int'
        elif c < 0.8:
            line = f'{n} .equ "' + r.choice(['Q', 'HI', 'ZZ', 'ABQX']) + '"'
            if r.random() < 0.5: self.later.append(line)
            else: self.emit(line)
            self.consts[n] = 'str'
        else:
            # reassignment generations
            self.emit(f'{n} = {r.randrange(0, 100)}'); self.consts[n] = 'int'; self.reassign.append(n)

    def reassign_one(self):
        n = self.r.choice(self.reassign)
        self.emit(f'{n} = {n}+{self.r.randrange(1, 9)}')

    def anon_def(self):
        if self.anon_pending:
            self.emit('+'); self.anon_pending -= 1
        else:
            self.emit('-'); self.anon_back = True

    def stmt(self, depth=0):
        r = self.r
        c = r.random()
        if c < 0.38: self.instr()
        elif c < 0.48: self.data()
        elif c < 0.56: self.label()
        elif c < 0.6:
            l = '@@' + self.name('l'); self.emit(f'{l}:'); self.locals.append(l)
        elif c < 0.64: self.anon_def()
        elif c < 0.7: self.const()
        elif c < 0.73 and self.reassign: self.reassign_one()
        elif c < 0.77: self.emit(f'.dsb {r.randrange(0, 6)}') if r.random() < 0.6 else self.emit(f'.dsw {r.randrange(1, 3)}')
        elif c < 0.8 and self.macros: self.emit(r.choice(self.macros)(self))
        elif c < 0.83 and depth < 1:
            self.emit(f'.rept {r.randrange(1, 3)}')
            for _ in range(r.randrange(1, 3)): self.instr() if r.random() < 0.6 else self.data()
            self.emit('.endm')
        elif c < 0.86 and self.blobs: self.emit(f'.incbin "{r.choice(self.blobs)}"')
        elif c < 0.89 and self.types:
            t, fields = r.choice(self.types)
            vals = ', '.join(str(r.randrange(0, 200)) for _ in fields)
            self.emit(f'.{t} {{ {vals} }}') if r.random() < 0.7 else self.emit(f'{self.name("V")} .{t} {{ {vals} }}')
        elif c < 0.91 and self.reassign and depth < 1:
            n = r.choice(self.reassign)
            self.emit(f'.undef {n}'); del self.consts[n]; self.reassign.remove(n)
        elif c < 0.93 and depth < 1:
            self.emit(f'.if {r.randrange(0, 2)}'); self.instr(); self.emit('.else'); self.data(); self.emit('.endif')
        elif c < 0.95 and depth < 1:
            n = self.name('I')
            self.emit(f'{n} = 0'); self.emit(f'.while {n} < {r.randrange(1, 3)}'); self.data() if r.random() < 0.5 else self.instr()
            self.emit(f'{n} = {n}+1'); self.emit('.endm')
        elif c < 0.96:
            l = self.name('B'); self.emit(f'.label {l}'); self.labels.append(l)
        elif c < 0.975 and depth < 1:
            # forward zero-page equate used before definition (narrowing)
            n = self.name('FZ'); self.emit(f'lda {n}'); self.later.append(f'{n} .equ ${r.randrange(0, 0x1ff):02X}')
        elif c < 0.985 and self.mac2:
            self.emit(r.choice(self.mac2)(self))
        else: self.instr()


def gen(rng):
    g = G(rng)
    g.close_soon = []
    g.anon_back = False
    g.macros = []
    g.mac2 = []
    g.ramtypes = []
    g.later = []
    g.ram_inst = []
    aux = {}
    g.blobs = []
    if rng.random() < 0.4:
        aux['map.txt'] = b'A=1\nB=2\nH=3\nI=4\nX=5\nQ=6\n'
        g.emit('.charmap "map.txt"')
    if rng.random() < 0.4:
        aux['blob.bin'] = bytes(rng.randrange(256) for _ in range(rng.randrange(1, 9)))
        g.blobs.append('blob.bin')
    if rng.random() < 0.3:
        aux['inc.asm'] = b'IncK .equ 9\n'
        g.emit('.include "inc.asm"'); g.consts['IncK'] = 'int'
    if rng.random() < 0.5:
        t = g.name('T')
        fields = [(g.name('f'), 1) for _ in range(rng.randrange(1, 4))]
        g.emit(f'.struc {t}')
        for f, _ in fields: g.emit(f'{f} .byte')
        g.emit('.ends')
        g.types.append((t, fields))
    if g.types and rng.random() < 0.5:
        t2 = g.name('U'); f1 = g.name('g'); f2 = g.name('g'); t1, fl = g.types[0]
        g.emit(f'.struc {t2}'); g.emit(f'{f1} .{t1}'); g.emit('.union'); g.emit(f'{f2} .word'); g.emit(f'{g.name("h")} .byte[3]'); g.emit('.ends'); g.emit('.ends')
        g.ramtypes.append((t2, [(f1, 1), (f2, 1)]))
    if rng.random() < 0.3:
        g.emit(f'.enum {g.name("E")}'); e1 = g.name('e'); e2 = g.name('e')
        g.emit(f'{e1} = 2'); g.emit(e2); g.emit('.ende'); g.consts[e1] = 'int'; g.consts[e2] = 'int'
    if rng.random() < 0.2:
        rn = g.name('R'); g.emit(f'.record {rn} hi:3, lo:5'); g.consts[f'{rn}::lo'] = 'int'
    if rng.random() < 0.5:
        m = g.name('M')
        g.emit(f'.macro {m} p')
        g.emit('@@m: lda #p'); g.emit('bne @@m'); g.emit('.endm')
        g.macros.append(lambda s, m=m: f'{m} {s.r.randrange(0, 200)}')
        m2 = g.name('N')
        g.emit(f'.macro {m2} pa, pb'); g.emit('lda pa'); g.emit(f'{m} pb'); g.emit('@@q: sta pa,x'); g.emit('beq @@q'); g.emit('.endm')
        g.mac2.append(lambda s, m2=m2: f'{m2} {s.addr_expr()}, {s.r.randrange(0, 9)}')
    # RAM
    g.emit('.dataseg')
    if rng.random() < 0.6:
        g.emit('.org $10' if rng.random() < 0.7 else '.org $00')
        for _ in range(rng.randrange(1, 4)):
            z = g.name('Z'); g.emit(f'{z} .byte' if rng.random() < 0.7 else f'{z} .word'); g.zp.append(z)
    g.emit(f'.org ${rng.randrange(0x200, 0x400):04X}')
    for _ in range(rng.randrange(1, 5)):
        v = g.name('W')
        c = rng.random()
        if c < 0.5: g.emit(f'{v} .byte[{rng.randrange(1, 9)}]')
        elif c < 0.7: g.emit(f'{v} .word')
        elif c < 0.85 and (g.types or g.ramtypes):
            t, fields = rng.choice(g.types + g.ramtypes); g.emit(f'{v} .{t}'); g.ram_inst.append((v, t, fields))
        else: g.emit(f'{v} .dsb {rng.randrange(1, 5)}')
        g.ram.append(v)
    g.emit('.codeseg')
    zpcode = rng.random() < 0.15
    g.emit(f'.org ${rng.choice([0x00C0, 0x00F0]) if zpcode else rng.choice([0x8000, 0xC000, 0x80F0, 0xFEF0]):04X}')
    g.label()
    for _ in range(rng.randrange(8, 45)):
        if rng.random() < 0.05 and not g.procs:
            p = g.name('P'); g.emit(f'.proc {p}'); g.locals = []
            for _ in range(rng.randrange(1, 6)): g.stmt(1)
            g.emit('rts'); g.emit('.endp'); g.procs.append(p); g.labels.append(p)
            continue
        if rng.random() < 0.04:
            g.emit('.dataseg'); v = g.name('W'); g.emit(f'{v} .byte'); g.ram.append(v); g.emit('.codeseg')
            continue
        g.stmt()
    while g.anon_pending: g.emit('+'); g.anon_pending -= 1; g.emit('nop')
    for l in list(g.fwd): g.emit(f'{l}: nop'); g.labels.append(l)
    g.fwd = []
    g.emit('rts')
    if rng.random() < 0.3:
        g.emit(f'.org $+{rng.randrange(0, 20)}' if rng.random() < 0.5 else '.org $FF00')
        g.emit(f'{g.name("Tail")}: .dw {rng.choice(g.labels)}')
    if rng.random() < 0.15 and g.procs:
        p = g.procs[0]; g.emit(f'.undef {p}'); g.emit(f'.proc {p}'); g.emit('.db 2'); g.emit('.endp')
    for l in g.later: g.emit(l)
    return '\n'.join(g.lines) + '\n', aux
