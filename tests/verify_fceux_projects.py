#!/usr/bin/env python3
"""Opt-in compatibility and NL-disabled timing checks against a baseline xasm.

Project sources and reference ROMs are read only. All outputs use a temporary
folder. Build both executables with the same compiler/options before invoking.
Example:
  python3 tests/verify_fceux_projects.py --baseline /tmp/xasm-base --candidate ./xasm \
      --source /path/to/donkey_kong.asm --source /path/to/kid_icarus.asm \
      --report /tmp/fceux-acceptance.json
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import statistics
import subprocess
import tempfile
import time


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def normalize(value):
    if isinstance(value, dict):
        return {key: normalize(child) for key, child in value.items() if key != 'timestamp_utc'}
    if isinstance(value, list):
        return [normalize(child) for child in value]
    return value


def modes(root):
    def file(name):
        return root / (name + '.json')
    return {
        'plain': ([], []),
        'xref': ([f'--xref={file("xref")}'], [file('xref')]),
        'xref-locals': ([f'--xref={file("xref")}', '--xref-include-locals=true', '--xref-include-anon=true'], [file('xref')]),
        'xref-full': ([f'--xref={file("xref")}', '--xref-include-locals=true', '--xref-include-anon=true',
                       '--xref-data=true', '--xref-include-owner=true', '--xref-instructions=true'], [file('xref')]),
        'listing': ([f'--listing={file("listing")}', '--listing-format=json'], [file('listing')]),
        'summary': (['--xref-summary', f'--xref-summary-output={file("summary")}', '--xref-summary-format=json'], [file('summary')]),
        'analyses': (['--analyze-index-patterns', f'--index-patterns-output={file("index")}',
                     '--data-consumers', f'--data-consumers-output={file("consumers")}',
                     '--analyze-data-coverage', f'--data-coverage-output={file("coverage")}'],
                    [file('index'), file('consumers'), file('coverage')]),
    }


def run(executable, source, root, flags):
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    start = time.perf_counter()
    result = subprocess.run([str(executable), '--pure-binary', str(source), '-o', str(root / 'output.prg'), *flags],
                            capture_output=True)
    elapsed = time.perf_counter() - start
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    cpu = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
    require(result.returncode == 0, f'{source.name}: {executable.name} failed: {result.stderr.decode()}')
    return result, {'wall_seconds': elapsed, 'cpu_seconds': cpu}


def artifacts(paths):
    return {path.name: normalize(json.loads(path.read_bytes())) for path in paths}


def nl_files(root):
    return {path.name: path.read_bytes() for path in root.glob('game.nes.*.nl')}


def clear_outputs(root, paths):
    for path in [root / 'output.prg', *paths, *root.glob('game.nes.*.nl')]:
        path.unlink(missing_ok=True)


def verify_project(baseline, candidate, source, root):
    available = modes(root)
    result, _ = run(baseline, source, root, [])
    binary = (root / 'output.prg').read_bytes()
    reference = source.parent.parent / 'reference' / (source.stem + '.nes')
    if reference.exists():
        rom = reference.read_bytes()
        require(rom[:4] == b'NES\x1a', f'{reference}: expected an iNES ROM')
        start = 16 + (512 if rom[6] & 4 else 0)
        require(binary == rom[start:start + rom[4] * 16384], f'{source}: PRG differs from reference ROM')
    nl = [f'--fceux-nl-rom-prefix={root}/game.nes.', f'--fceux-nl-ram-output={root}/game.nes.ram.nl']
    if len(binary) == 16384:
        nl.append('--fceux-nl-mirror-16k')
    run(candidate, source, root, nl)
    expected_nl = nl_files(root)
    expected_banks = (len(binary) + 16383) // 16384
    expected_paths = {f'game.nes.{bank:X}.nl' for bank in range(expected_banks)} | {'game.nes.ram.nl'}
    require(set(expected_nl) == expected_paths, f'{source}: incorrect physical bank files')
    for path, contents in expected_nl.items():
        addresses = set()
        for line in contents.decode().splitlines():
            match = re.fullmatch(r'\$([0-9A-F]{4})#([^#]+)#([^#]*)', line)
            require(match is not None, f'{path}: malformed NL entry: {line}')
            address = int(match[1], 16)
            require(address not in addresses, f'{path}: duplicate address {address:04X}')
            require((address < 0x8000) == path.endswith('.ram.nl'), f'{path}: incorrect address range')
            addresses.add(address)
    checks = []
    for name, (flags, paths) in available.items():
        clear_outputs(root, paths)
        before, _ = run(baseline, source, root, flags)
        expected = artifacts(paths)
        for enabled in (False, True):
            clear_outputs(root, paths)
            after, _ = run(candidate, source, root, [*flags, *(nl if enabled else [])])
            require((root / 'output.prg').read_bytes() == binary, f'{source}: {name}, NL={enabled}: binary changed')
            require(after.stdout == before.stdout, f'{source}: {name}, NL={enabled}: stdout changed')
            require(after.stderr == before.stderr, f'{source}: {name}, NL={enabled}: diagnostics changed')
            actual = artifacts(paths)
            require(actual == expected, f'{source}: {name}, NL={enabled}: changed artifacts: '
                    + str([path for path in actual if actual[path] != expected[path]]))
            if enabled:
                require(nl_files(root) == expected_nl, f'{source}: {name}: NL output depends on analysis flags')
            else:
                require(not nl_files(root), f'{source}: {name}: NL files written without an NL flag')
        checks.append(name)
        print(f'{source.stem}: compatibility {name} passed (NL off/on)', flush=True)
    return {'source': str(source), 'bytes': len(binary), 'sha256': hashlib.sha256(binary).hexdigest(),
            'reference_prg_checked': reference.exists(), 'banks': expected_banks,
            'nl_entries': {name: len(contents.splitlines()) for name, contents in expected_nl.items()},
            'compatibility_modes': checks}


def benchmark(baseline, candidate, source, root, repeats, limit, mode_names):
    results = {}
    for name in mode_names:
        flags, _ = modes(root)[name]
        samples = {'baseline': [], 'candidate': []}
        # Warm the executable and input caches, then alternate execution order.
        for executable in (baseline, candidate):
            run(executable, source, root, flags)
        for iteration in range(repeats):
            order = [('baseline', baseline), ('candidate', candidate)]
            if iteration % 2:
                order.reverse()
            for label, executable in order:
                _, sample = run(executable, source, root, flags)
                samples[label].append(sample)
            print(f'{source.stem}: {name} timing pair {iteration + 1}/{repeats}', flush=True)
        medians = {label: {metric: statistics.median(sample[metric] for sample in data)
                           for metric in ('wall_seconds', 'cpu_seconds')} for label, data in samples.items()}
        ratios = {metric: medians['candidate'][metric] / medians['baseline'][metric]
                  for metric in ('wall_seconds', 'cpu_seconds')}
        # CPU time avoids flagging unrelated scheduler load as an implementation
        # regression. A 20 ms allowance covers timer/process startup variation.
        passed = medians['candidate']['cpu_seconds'] <= medians['baseline']['cpu_seconds'] * limit + 0.020
        results[name] = {'nl_enabled': False, 'samples': samples, 'medians': medians,
                         'candidate_over_baseline': ratios, 'passed': passed}
        print(f'{source.stem}: {name} CPU ratio {ratios["cpu_seconds"]:.3f}, wall ratio {ratios["wall_seconds"]:.3f}', flush=True)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--source', type=Path, action='append', required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--repeats', type=int, default=5)
    parser.add_argument('--max-cpu-ratio', type=float, default=1.20)
    parser.add_argument('--timing-mode', action='append', choices=tuple(modes(Path('.'))),
                        help='Repeat for several modes; defaults to plain, xref, xref-full, listing, summary.')
    args = parser.parse_args()
    require(args.repeats >= 3, 'Use at least three paired timing samples')
    baseline, candidate = args.baseline.resolve(), args.candidate.resolve()
    report = {'baseline': str(baseline), 'candidate': str(candidate),
              'baseline_sha256': hashlib.sha256(baseline.read_bytes()).hexdigest(),
              'candidate_sha256': hashlib.sha256(candidate.read_bytes()).hexdigest(),
              'repeats': args.repeats,
              'max_cpu_ratio': args.max_cpu_ratio, 'timing_allowance_seconds': 0.020, 'projects': []}
    with tempfile.TemporaryDirectory(prefix='xasm-nl-projects-') as directory:
        work = []
        for index, source in enumerate(args.source):
            source = source.resolve()
            root = Path(directory) / str(index)
            root.mkdir()
            project = verify_project(baseline, candidate, source, root)
            report['projects'].append(project)
            work.append((source, root, project))
        # Run timings after compatibility work; do not run benchmarks concurrently.
        for source, root, project in work:
            project['performance'] = benchmark(baseline, candidate, source, root, args.repeats,
                                                args.max_cpu_ratio, args.timing_mode or
                                                ['plain', 'xref', 'xref-full', 'listing', 'summary'])
    report['passed'] = all(result['passed'] for project in report['projects']
                           for result in project['performance'].values())
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    require(report['passed'], f'NL-disabled performance threshold exceeded; see {args.report}')
    print(f'All project acceptance checks passed. Report: {args.report}', flush=True)


if __name__ == '__main__':
    main()
