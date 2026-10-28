"""Multi-unit generator adapted from the preserved linki56.py; see provenance.json."""

def generate(rng):
    nu=rng.randrange(2,5); units=[]
    exports={}  # name -> (unit, kind) kind in code, zp, ram, num
    for u in range(nu):
        names={'code':[f'C{u}_{i}' for i in range(rng.randrange(1,4))],
               'zp':[f'Z{u}_{i}' for i in range(rng.randrange(0,3))],
               'ram':[f'R{u}_{i}' for i in range(rng.randrange(0,4))],
               'num':[f'N{u}_{i}' for i in range(rng.randrange(0,3))]}
        for k,v in names.items():
            for n in v: exports[n]=(u,k)
        units.append(names)
    srcs=[]
    for u,names in enumerate(units):
        foreign=[n for n,(o,k) in exports.items() if o!=u]
        explicit=[n for n in foreign if rng.random()<0.7]
        lines=[]
        pub=[n for k in names.values() for n in k]
        if pub: lines.append('.public '+', '.join(pub))
        for n in explicit:
            k=exports[n][1]; lines.append(f'.extrn {n}:'+('byte' if k in ('zp','num') else 'word'))
        nums={}
        for n in names['num']:
            v=rng.choice([rng.randrange(0,8),rng.randrange(0,256),rng.randrange(0,70000),-rng.randrange(1,300)])
            nums[n]=v; lines.append(f'{n} .equ {v}')
        if names['zp'] or names['ram']:
            lines.append('.dataseg')
            for n in names['zp']:
                lines.append(f'.zeropage {n} .byte'+(f'[{rng.randrange(1,4)}]' if rng.random()<0.5 else ''))
            for n in names['ram']:
                lines.append(f'{n} '+rng.choice(['.byte','.word',f'.byte[{rng.randrange(1,40)}]',f'.word[{rng.randrange(1,9)}]']))
                if rng.random()<0.3: lines.append(f'.align {n} {1<<rng.randrange(1,6)}')
            if rng.random()<0.3: lines.append(f'.dsb {rng.randrange(1,5)}')
            lines.append('.codeseg')
        local=[f'L{u}_{i}' for i in range(rng.randrange(1,4))]
        symbols=foreign+[n for k in ('zp','ram','code') for n in names[k]]+local
        numeric=[n for n in foreign if exports[n][1]=='num']+names['num']
        labels_left=names['code']+local; rng.shuffle(labels_left)
        body=[]
        def operand():
            s=rng.choice(symbols)
            r=rng.random()
            if r<0.15: return f'{s}+{rng.randrange(1,4)}'
            if r<0.2 and numeric: return rng.choice(numeric)
            return s
        for _ in range(rng.randrange(4,14)):
            if labels_left and rng.random()<0.35: body.append(labels_left.pop()+':')
            k=rng.randrange(12)
            if k<3: body.append(rng.choice(['lda','sta','ldx','ldy','cmp','inc','adc'])+' '+operand())
            elif k==3: body.append(rng.choice(['lda','sta'])+' '+operand()+','+rng.choice(['x','y']))
            elif k==4:
                sel=rng.choice(['<','>','^',''])
                body.append('lda #'+sel+(rng.choice(symbols) if sel=='^' else ('('+operand()+')' if rng.random()<0.3 else operand())))
            elif k==5: body.append('.db '+', '.join(rng.choice(['<','>','^'])+rng.choice(symbols) if rng.random()<0.6 else str(rng.randrange(0,256)) for _ in range(rng.randrange(1,4))))
            elif k==6: body.append('.dw '+', '.join(operand() for _ in range(rng.randrange(1,3))))
            elif k==7 and numeric: body.append(f'.dsb (({rng.choice(numeric)})&3)+1')
            elif k==8: body.append(rng.choice(['bne','beq','bcc'])+' '+rng.choice(local))
            elif k==9: body.append(rng.choice(['jmp','jsr'])+' '+rng.choice([n for n,(o,kk) in exports.items() if kk=='code']+local))
            elif k==10: body.append('.db "AB", 7')
            else: body.append(rng.choice(['nop','rts','tax','pha']))
        for n in labels_left: body.append(n+': rts')
        lines+=body+['rts']
        srcs.append('\n'.join(lines)+'\n')
    order=list(range(nu)); rng.shuffle(order)
    scr=['output{file=out.rom}']
    if rng.random()<0.8: scr.append('ram{start=$0,end=$100}')
    if rng.random()<0.8: scr.append('ram{start=$200,end=$800}')
    if rng.random()<0.3: scr.append('copy{file=hdr}')
    banked=rng.random()<0.6
    for i,u in enumerate(order):
        if banked and (i==0 or rng.random()<0.5):
            scr.append(f'bank{{size=${rng.choice([0x400,0x1000,0x4000]):X},origin=${rng.choice([0x8000,0xC000,0x6000]):X}}}')
        scr.append(f'link{{file={u}.o'+(f',origin=${rng.choice([0x8000,0xC000,0x300]):X}' if (not banked and i==0) or rng.random()<0.15 else '')+'}')
        if rng.random()<0.25: scr.append(rng.choice([f'pad{{size={rng.randrange(0,6)}}}',f'pad{{offset=${rng.randrange(0x100,0x180):X}}}',f'pad{{origin=$C{rng.randrange(0x100,0x180):03X}}}']))
    return srcs,'\n'.join(scr)+'\n'
