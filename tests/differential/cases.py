"""Deterministic inputs and lexical feature coverage (not execution coverage)."""
from collections import Counter
from pathlib import Path
import random
import re

import analysis_gen
import assignment_gen
import link_gen
from products import CONFIGS

FEATURES = {
    'reassignment': r'^\w+\s*=\s*', 'undef': r'\.undef\b', 'aliases': r'\.equ\s+\w',
    'local_labels': r'@@', 'anonymous_labels': r'^\s*[+-]\s*:?\s*$',
    'types': r'\.(struc|union|record|enum)\b', 'macros': r'\.macro\b',
    'loops': r'\.(while|rept)\b', 'conditionals': r'\.if\b',
    'current_pc': r'\$(?![0-9a-fA-F])', 'sizeof': r'\bsizeof\(',
    'segment_switches': r'\.(data|code)seg\b', 'forward_narrowing': r'\bFZ\d+\b',
    'char': r'\.char\b', 'strings': r'\.equ\s+"|\.(db|dw|dd|char|byte)\s+[^\n]*"',
    'charmap': r'\.charmap\b', 'relative_org': r'\.org\s+\$\+',
    'includes': r'\.include\b', 'incbin': r'\.incbin\b', 'procedures': r'\.proc\b',
    'storage': r'\.ds[bw]\b', 'imports': r'\.extrn\b', 'exports': r'\.public\b',
    'ram_alignment': r'\.align\b', 'banks': r'\bbank\{', 'pads': r'\bpad\{',
    'copies': r'\bcopy\{', 'ram_blocks': r'\bram\{',
}


def features(inputs):
    source = '\n'.join(data.decode() for name, data in inputs.items()
                       if name.endswith(('.asm', '.s')))
    found = Counter({key: 1 for key, pattern in FEATURES.items()
                     if re.search(pattern, source, re.M | re.I)})
    for left, right in (('aliases', 'local_labels'), ('reassignment', 'undef'),
                        ('types', 'sizeof'), ('char', 'strings'), ('macros', 'local_labels'),
                        ('storage', 'imports'), ('banks', 'pads')):
        if left in found and right in found:
            found[left + '+' + right] = 1
    return found


def generate(seed, binary_count, link_count):
    cases = []
    for index in range(binary_count):
        rng = random.Random(f'{seed}:binary:{index}')
        if index % 10 == 9:
            source, auxiliary = assignment_gen.generate(rng), {}
            generator = 'assignment'
        else:
            source, auxiliary = analysis_gen.gen(rng)
            generator = 'analysis'
        cases.append(({'id': f'binary-{index:03}', 'kind': 'binary', 'generator': generator,
                       'existing': index % 2 == 0}, {'p.asm': source.encode(), **auxiliary}))
    for index in range(link_count):
        rng = random.Random(f'{seed}:link:{index}')
        sources, script = link_gen.generate(rng)
        inputs = {f'{i}.asm': source.encode() for i, source in enumerate(sources)}
        inputs.update({'link.s': script.encode(), 'hdr': bytes([index & 255, 0x4e, 0x45, 0x53])})
        cases.append(({'id': f'link-{index:03}', 'kind': 'link', 'generator': 'multi-unit',
                       'existing': index % 2 == 0}, inputs))
    return cases


def controls():
    root = Path(__file__).with_name('fixtures')
    cases = []
    for name, extra in (
            ('analysis', {'must_accept': True, 'expected_instructions': 8}),
            ('captures', {'must_accept': True, 'expected_hex': '05000600050001000100'}),
            ('invalid', {'must_reject': True})):
        inputs = {'p.asm': (root / (name + '.asm')).read_bytes(),
                  'map.txt': (root / 'map.txt').read_bytes()}
        cases.append(({'id': 'control-' + name, 'kind': 'binary', 'generator': 'fixture',
                       'existing': True, **extra}, inputs))
    cases.append(({'id': 'control-link-strings', 'kind': 'link', 'generator': 'fixture',
                   'existing': True, 'must_accept': True, 'expected_hex': '4849000000ad0a800a8060'},
                  {name: (root / 'link' / name).read_bytes() for name in ('0.asm', '1.asm', 'link.s')}))
    cases.append(({'id': 'control-mirror', 'kind': 'binary', 'generator': 'fixture',
                   'existing': False, 'must_accept': True,
                   'configs': {'mirror': ['--fceux-nl-rom-prefix=rom', '--fceux-nl-mirror-16k',
                                          '--listing=l.json', '--listing-format=json']}},
                  {'p.asm': b'.org $8000\nEntry: nop\n.dsb 16383\n'}))
    return cases


def jobs(case):
    configs = case.get('configs', CONFIGS if case['kind'] == 'binary' else {'link': []})
    for config, args in configs.items():
        yield {**case, 'config': config, 'args': args}
