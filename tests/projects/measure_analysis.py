#!/usr/bin/env python3
"""Paired Unix producer/consumer measurements on the frozen baseline inputs.

Default: bounded five-pair survey, not a formal performance pass. For the formal
sampling schedule use --pairs 15 --batch-seconds 2 on an idle machine. Correctness
and cross-version fact parity are checked separately; every timed run checks its
exit status and the exact frozen PRG. No project checkout is written.
"""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import time

MODES = {
    'plain': [],
    'text': ['--listing={out}/listing.txt'],
    'json': ['--listing={out}/listing.json', '--listing-format=json', '--listing-version=2'],
    'ndjson': ['--listing={out}/listing.ndjson', '--listing-format=ndjson', '--listing-version=2'],
    'xref': ['--xref={out}/xref.json'],
    'xref-owner': ['--xref={out}/xref.json', '--xref-data=true', '--xref-include-owner=true'],
    'xref-records': ['--xref={out}/xref.json', '--xref-instructions=true'],
    'xref-combined': ['--xref={out}/xref.json', '--xref-data=true', '--xref-include-owner=true',
                      '--xref-instructions=true', '--xref-include-locals=true', '--xref-include-anon=true'],
    'records': ['--instruction-records-output={out}/records.json', '--dependency-manifest={out}/deps.json'],
    'summary': ['--xref-summary', '--xref-summary-output={out}/summary.json'],
    'index': ['--analyze-index-patterns', '--index-patterns-output={out}/index.json'],
    'consumers': ['--data-consumers', '--data-consumers-output={out}/consumers.json'],
    'coverage': ['--analyze-data-coverage', '--data-coverage-output={out}/coverage.json'],
    'compare': ['--compare={ref}', '--compare-format=json', '--compare-cpu-base=$C000'],
    'audit': ['--audit-raw-addresses', '--audit-rom-range=$C000-$FFFF', '--audit-output-format=json'],
    'fceux': ['--fceux-nl-rom-prefix={out}/rom', '--fceux-nl-ram-output={out}/ram.nl'],
}
for fmt in ('text', 'csv'):
    MODES['xref-' + fmt] = ['--xref={out}/xref', '--xref-format=' + fmt]
for name, option in (('summary', 'xref-summary'), ('index', 'index-patterns'),
                     ('consumers', 'data-consumers'), ('coverage', 'data-coverage')):
    for fmt in ('text', 'ndjson'):
        MODES[name + '-' + fmt] = MODES[name] + ['--' + option + '-format=' + fmt]
PROFILES = ('ci-data-v1', 'ci-instructions-v1', 'inventory-instructions-v1',
            'process-instructions-v1', 'maturity-instructions-v1', 'instructions-v1',
            'data-listing-v1', 'pass-prep-instructions-v1')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('capture', 'baseline', 'candidate', 'consumer', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--pairs', type=int, default=5)
    p.add_argument('--batch-seconds', type=float, default=0)
    p.add_argument('--bundles', action='store_true')
    p.add_argument('--mode', action='append')
    args = p.parse_args()
    if args.pairs < 1 or args.batch_seconds < 0: p.error('invalid sampling count')
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    for name in ('home', 'tmp'): (root / name).mkdir()
    env = dict(PATH='/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin', HOME=str(root / 'home'),
               TMPDIR=str(root / 'tmp'), LC_ALL='C', TZ='UTC', PYTHONDONTWRITEBYTECODE='1')
    inputs = args.capture.resolve() / 'inputs/nesrev'
    tools = {'baseline': args.baseline.resolve(), 'candidate': args.candidate.resolve()}
    consumers = {'baseline': inputs / 'scripts/analysis_bundle.py', 'candidate': args.consumer.resolve()}
    report = dict(kind='formal-sampling' if args.pairs >= 15 and args.batch_seconds >= 2 else 'survey',
                  pairs=args.pairs, batch_seconds=args.batch_seconds, environment=env,
                  tools={k: {'path': str(v), 'sha256': digest(v)} for k, v in tools.items()},
                  consumers={k: {'path': str(v), 'sha256': digest(v)} for k, v in consumers.items()},
                  configurations=[])
    sequence = 0

    def timed(command, directory, environment):
        nonlocal sequence
        sequence += 1
        start = time.monotonic()
        stderr = directory / f'timed-{sequence}.stderr'
        with stderr.open('wb') as err, (directory / f'timed-{sequence}.stdout').open('wb') as out:
            process = subprocess.Popen(command, cwd=inputs, env=environment, stdout=out, stderr=err)
            _, status, usage = os.wait4(process.pid, 0)
            process.returncode = os.waitstatus_to_exitcode(status)
        elapsed = time.monotonic() - start
        if process.returncode:
            raise RuntimeError((command, process.returncode, stderr.read_text()[-2000:]))
        rss = usage.ru_maxrss * (1 if sys.platform == 'darwin' else 1024)
        return dict(elapsed=elapsed, cpu=usage.ru_utime + usage.ru_stime, rss=rss, command=command)

    for project in ('metroid', 'zelda'):
        source = inputs / 'projects' / project / 'asm' / (project + '.asm')
        reference = args.capture / (project + '-reference-prg.bin')
        expected = reference.read_bytes()
        names = args.mode or (PROFILES if args.bundles else MODES)
        for mode in names:
            stages = ('producer', 'consumer', 'bundle') if args.bundles else ('producer',)
            for stage in stages:
                config = dict(project=project, mode=mode, stage=stage, source_sha256=digest(source), samples=[])
                directory = root / project / mode / stage
                directory.mkdir(parents=True)
                serial = 0

                def once(version):
                    nonlocal serial
                    serial += 1
                    out = directory / f'{version}-{serial:06d}'
                    out.mkdir()
                    environment = dict(env, XASM_BIN=str(tools[version]))
                    if args.bundles:
                        script = str(consumers[version])
                        conf = source.parents[1] / 'project.conf'
                        if mode in ('instructions-v1', 'data-listing-v1'):
                            prepare = ['prepare-source', str(out), str(source)]
                        else:
                            prepare = ['prepare', str(out), project, str(conf), digest(conf), str(source), '$C000-$FFFF', '$C000']
                        subprocess.run([sys.executable, script, *prepare, '--profile', mode], cwd=inputs,
                                       env=environment, check=True, capture_output=True)
                        produce = [sys.executable, script, 'produce', str(out), str(source), str(out / 'binary.bin'), '--profile', mode]
                        validate = [sys.executable, str(Path(__file__).resolve()), '--read-bundle',
                                    script, str(out / 'bundle.json'), str(source)]
                        if stage == 'consumer':
                            subprocess.run(produce, cwd=inputs, env=environment, capture_output=True, check=True)
                            sample = timed(validate, directory, environment)
                        elif stage == 'bundle':
                            # produce includes publication/validation; validate is the downstream reader.
                            a = timed(produce, directory, environment); b = timed(validate, directory, environment)
                            sample = dict(elapsed=a['elapsed']+b['elapsed'], cpu=a['cpu']+b['cpu'],
                                          rss=max(a['rss'], b['rss']), command=[produce, validate])
                        else:
                            # Use the exact profile argv; profile preparation is outside producer timing.
                            # Obtain recorded producer argv from one untimed bundle production.
                            subprocess.run(produce, cwd=inputs, env=environment, capture_output=True, check=True)
                            manifest = json.loads((out / 'bundle.json').read_text())
                            sample = timed(manifest['argv'], directory, environment)
                    else:
                        command = [str(tools[version]), '--pure-binary', str(source), '-o', str(out / 'binary.bin'),
                                   *[flag.format(out=out, ref=reference) for flag in MODES[mode]
                                     if version == "baseline" or not flag.startswith("--listing-version=")]]
                        sample = timed(command, directory, environment)
                    if (out / 'binary.bin').read_bytes() != expected: raise RuntimeError('PRG mismatch: ' + str(out))
                    sample['artifacts'] = {f.name: f.stat().st_size for f in out.iterdir() if f.is_file()}
                    shutil.rmtree(out)  # This script's newly created per-run directory only.
                    return sample

                warm = [once(v) for _ in range(2) for v in tools]
                count = max(1, math.ceil(args.batch_seconds / max(s['elapsed'] for s in warm)))
                config['batch_count'] = count
                for pair in range(args.pairs):
                    order = tuple(tools) if pair % 2 == 0 else tuple(reversed(tools))
                    record = dict(pair=pair, order=order)
                    for version in order:
                        samples = [once(version) for _ in range(count)]
                        record[version] = dict(elapsed=sum(s['elapsed'] for s in samples)/count,
                                               cpu=sum(s['cpu'] for s in samples)/count,
                                               rss=max(s['rss'] for s in samples), runs=samples)
                    config['samples'].append(record)
                config['median_ratios'] = {key: statistics.median(s['candidate'][key]/s['baseline'][key]
                                         for s in config['samples']) for key in ('elapsed','cpu','rss')}
                config['median_ms_delta'] = {key: 1000*statistics.median(s['candidate'][key]-s['baseline'][key]
                                            for s in config['samples']) for key in ('elapsed','cpu')}
                report['configurations'].append(config)
                (root / 'report.json').write_text(json.dumps(report, indent=2)+'\n')
                print(project, mode, stage, config['median_ratios'], config['median_ms_delta'], flush=True)


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--read-bundle':
        script, manifest, source = sys.argv[2:]
        sys.path.insert(0, str(Path(script).parent))
        spec = importlib.util.spec_from_file_location('analysis_bundle', script)
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        bundle = module.Bundle(manifest, source)
        payloads = {name: bundle.load(name) for name in bundle.data['outputs'] if name != 'binary'}
        print(json.dumps({name: len(payload) for name, payload in payloads.items()}, sort_keys=True))
    else:
        main()
