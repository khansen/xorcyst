"""Reassignment generator adapted from assigngen.py; see provenance.json."""

def generate(rng):
    out=[]; forward=rng.random()<0.6; defined=set(); caps=0
    if not forward: out.append('K .equ 5')
    for _ in range(rng.randrange(3,12)):
        k=rng.randrange(9); s=rng.choice('ST')
        if k<4:
            rhs=rng.choice(['K','3',f'{s}+1',f'{s}*2','S+T','T-1','K+2'] if defined else ['K','3','K+1'])
            if any(c in rhs for c in 'ST') and not ({'S','T'}&set(rhs))<=defined: rhs='K'
            out.append(f'{s} = {rhs}'); defined.add(s)
        elif k<7 and s in defined: out.append(f'.dw {s}')
        elif k==7 and s in defined: caps+=1; out.append(f'X{caps} .equ {s}'); out.append(f'.dw X{caps}')
        elif k==8 and s in defined and rng.random()<0.3: out.append(f'.undef {s}'); defined.discard(s)
    out.append('.dw '+', '.join(sorted(defined) or ['0']))
    if forward: out.append('K .equ 5')
    return '\n'.join(out)+'\n'
