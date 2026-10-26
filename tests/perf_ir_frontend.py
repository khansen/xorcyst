#!/usr/bin/env python3
"""Paired I3 plain-output gate; requires the external frozen D1 inputs.

See doc/performance.md. This is deliberately outside make check: it
needs release binaries, frozen private project inputs and an idle machine.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import sys
import time


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def interval(ratios):
    logs = [math.log(x) for x in ratios]
    rng = random.Random(20261024)
    samples = sorted(statistics.median(rng.choices(logs, k=len(logs)))
                     for _ in range(10000))
    return dict(median=math.exp(statistics.median(logs)),
                ci95=[math.exp(samples[249]), math.exp(samples[9749])])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--pairs', type=int, default=15)
    parser.add_argument('--reverse', action='store_true')
    args = parser.parse_args()
    if args.pairs < 15:
        parser.error('the gate requires at least 15 pairs')
    args.output.mkdir(parents=True, exist_ok=False)
    home = args.output/'home'
    tmp = args.output/'tmp'
    home.mkdir()
    tmp.mkdir()
    environment = dict(PATH='/usr/bin:/bin', HOME=str(home), TMPDIR=str(tmp),
                       LC_ALL='C', TZ='UTC')
    tools = {'v1': args.baseline.resolve(), 'core': args.candidate.resolve()}
    result = dict(environment=environment, platform=platform.platform(),
                  tools={k: dict(path=str(v), sha256=sha(v)) for k, v in tools.items()},
                  bootstrap_seed=20261024, bootstrap_resamples=10000,
                  rss_units='bytes', projects={})
    for game in ('metroid', 'zelda'):
        source = args.fixtures/f'inputs/nesrev/projects/{game}/asm/{game}.asm'
        reference = args.fixtures/f'{game}-reference-prg.bin'
        expected = reference.read_bytes()
        directory = args.output/game
        directory.mkdir()
        record = dict(source=str(source), source_sha256=sha(source),
                      reference_sha256=sha(reference), bytes=len(expected), samples=[])
        result['projects'][game] = record
        diagnostics = None

        def run_batch(which, label, count):
            nonlocal diagnostics
            paths = [directory/f'{which}-{label}-{n}.bin' for n in range(count)]
            errors = [p.with_suffix('.stderr') for p in paths]
            handles = [p.open('wb') for p in errors]
            cpu = 0.0
            peak = 0
            statuses = []
            start = time.perf_counter()
            for output, handle in zip(paths, handles):
                command = [str(tools[which]), '--pure-binary', str(source), '-o', str(output)]
                child = subprocess.Popen(command, env=environment, cwd=directory,
                                         stdout=subprocess.DEVNULL, stderr=handle)
                _, status, usage = os.wait4(child.pid, 0)
                child.returncode = os.waitstatus_to_exitcode(status)
                statuses.append(child.returncode)
                cpu += usage.ru_utime + usage.ru_stime
                peak = max(peak, usage.ru_maxrss * (1 if sys.platform == 'darwin' else 1024))
            elapsed = time.perf_counter() - start
            for handle in handles:
                handle.close()
            for output, error, status in zip(paths, errors, statuses):
                assert status == 0, (status, error.read_text())
                assert output.read_bytes() == expected, f'byte mismatch: {output}'
                actual = error.read_bytes()
                if diagnostics is None:
                    diagnostics = actual
                    (directory/'diagnostics.txt').write_bytes(actual)
                assert actual == diagnostics, f'diagnostic mismatch: {error}'
                output.unlink()
                error.unlink()
            return dict(elapsed=elapsed, cpu=cpu, peak_rss=peak, repetitions=count)

        warmups = {k: [run_batch(k, f'warm{n}', 1) for n in range(2)] for k in tools}
        # Leave headroom above two seconds, including for the faster partner.
        batch = max(1, math.ceil(2.5/min(x['elapsed'] for xs in warmups.values() for x in xs)))
        record.update(warmups=warmups, batch=batch,
                      command=['{tool}', '--pure-binary', str(source), '-o', '{fresh_output}'])
        for pair in range(args.pairs):
            order = ['core', 'v1'] if (pair + args.reverse) % 2 else ['v1', 'core']
            sample = dict(order=order)
            for which in order:
                sample[which] = run_batch(which, str(pair), batch)
            record['samples'].append(sample)
            (args.output/'results.json').write_text(json.dumps(result, indent=2)+'\n')
            print(game, pair + 1, 'elapsed ratio',
                  sample['core']['elapsed']/sample['v1']['elapsed'], flush=True)
        assert sha(source) == record['source_sha256'], 'source changed during timing'
        assert sha(reference) == record['reference_sha256'], 'reference changed during timing'
        for key, tool in tools.items():
            assert sha(tool) == result['tools'][key]['sha256'], 'tool changed during timing'
        record['statistics'] = {
            metric: interval([s['core'][metric]/s['v1'][metric] for s in record['samples']])
            for metric in ('elapsed', 'cpu')}
        peaks = {k: max(s[k]['peak_rss'] for s in record['samples']) for k in tools}
        ratios = [s['core']['peak_rss']/s['v1']['peak_rss'] for s in record['samples']]
        record['memory'] = dict(maximum_bytes=peaks, maximum_ratio=peaks['core']/peaks['v1'],
                                median_paired_ratio=statistics.median(ratios),
                                worst_paired_ratio=max(ratios), limit_exclusive=1.30)
        record['memory']['passes'] = max(ratios) < 1.30 and peaks['core']/peaks['v1'] < 1.30
        print(game, record['statistics'], flush=True)
        (args.output/'results.json').write_text(json.dumps(result, indent=2)+'\n')


if __name__ == '__main__':
    main()
