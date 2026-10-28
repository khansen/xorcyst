#!/usr/bin/env python3
"""Run the portable, fixed-seed XORcyst differential; retain coverage and reproducers."""
import argparse
import json
import math
from pathlib import Path
import platform
import shutil
import signal
import sys
import time
import traceback

from build_reference import build_reference, default_cache
from cases import controls, generate, jobs
from common import compare, execute, interrupted, save_raw, summary, tool_identity
from products import CONFIGS
from reporting import aggregate, write_json, zero_success_errors


def tools_at(directory):
    return {name: tool_identity(directory / name) for name in ('xasm', 'xlnk')}


def save_pair(output, case, inputs, tools, results, differences, complete):
    directory = output / ('failures' if complete else 'incomplete') / (case['id'] + '-' + case['config'])
    directory.mkdir(parents=True)
    for name, value in (('case', case), ('tools', tools), ('differences', differences),
                        ('completion', {'complete': complete, 'reason': None if complete else 'budget'})):
        write_json(directory / (name + '.json'), value)
    for name, data in inputs.items():
        path = directory / 'inputs' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    for side, result in results.items():
        save_raw(directory / side, result)
    return str(directory.relative_to(output))


def run(args):
    output = args.output.resolve()
    # Never replace an earlier run or count its products as new successes.
    output.mkdir(parents=True, exist_ok=False)
    report = {'schema': 2, 'state': 'preparing', 'seed': args.seed, 'tools': {},
              'host': platform.platform(), 'python': platform.python_version(),
              'budget_seconds': args.budget, 'invocation_timeout_seconds': args.timeout,
              'requested': {'binary_cases': args.binary_cases, 'link_cases': args.link_cases},
              'configs': CONFIGS, 'results': [], 'omitted': [], 'coverage': {},
              'failures': [], 'incomplete_bundles': [], 'coverage_errors': [],
              'complete': False, 'passed': False,
              'exclusions': [
                  'Object bytes are retained but not compared: each toolchain links its own objects.',
                  'The random multi-unit generator has no imported strings; control-link-strings covers them.',
                  'Lexical feature counts describe source presence, not executed expansion branches.',
                  'Allocation/publication injection and object mutation remain separate test suites.',
                  'This bounded matrix is not the Cartesian product of every CLI option.'],
              'normalization': [
                  'x.json build.timestamp_utc is validated and replaced with a clock marker.',
                  'd.json producer version, executable path/size/hash and argv[0] are validated against each tool then marked.',
                  'JSON object key order and whitespace are incidental; other fields, streams and bytes compare exactly.']}
    cases, workload = [], []
    started = None
    work = output / 'work'

    def checkpoint():
        if workload:
            aggregate(report, cases, workload)
        report['elapsed_seconds'] = time.monotonic() - started if started is not None else 0
        report['complete'] = report['state'] == 'finished' and not report['omitted']
        report['coverage_errors'] = zero_success_errors(report['coverage']) if report['complete'] else []
        report['passed'] = report['complete'] and not report['failures'] and not report['coverage_errors']
        write_json(output / 'report.json', report)

    # A failed build/tool lookup must leave a report too.
    checkpoint()
    try:
        baseline = args.baseline_dir or build_reference(args.cache)
        tools = {'baseline': tools_at(baseline), 'candidate': tools_at(args.candidate_dir)}
        report['tools'] = tools
        if (baseline / 'build.json').exists():
            report['reference_build'] = json.loads((baseline / 'build.json').read_text())
        started = time.monotonic()
        deadline = started + args.budget
        cases = [*controls(), *generate(args.seed, args.binary_cases, args.link_cases)]
        for case, inputs in cases:
            if case['kind'] == 'binary':
                inputs.update({'ref.bin': bytes(16), 'match.bin': bytes(16)})
            directory = output / 'cases' / case['id']
            directory.mkdir(parents=True)
            for name, data in inputs.items():
                (directory / name).write_bytes(data)
            write_json(directory / 'case.json', case)
            workload.extend((job, inputs) for job in jobs(case))
        report['planned'] = {'generated_binary_programs': args.binary_cases,
                             'generated_link_programs': args.link_cases, 'controls': len(controls()),
                             'comparisons': len(workload),
                             'invocations_max': sum(2 * (1 if job['kind'] == 'binary' else
                                                   1 + sum(n.endswith('.asm') for n in inputs))
                                                    for job, inputs in workload)}
        report['state'] = 'running'
        checkpoint()
        # Append and flush each complete pair. Periodic atomic snapshots keep
        # report.json readable even after SIGKILL; JSONL retains newer pairs.
        with (output / 'results.jsonl').open('w') as journal:
            for index, (case, inputs) in enumerate(workload):
                if time.monotonic() >= deadline:
                    report['stop_reason'] = 'wall-clock budget exhausted'
                    break
                report['active'] = case['id'] + '/' + case['config']
                write_json(output / 'active.json', {'case': case, 'pair_index': index})
                results = {side: execute(case, inputs, tools[side], work, args.timeout, deadline)
                           for side in ('baseline', 'candidate')}
                budget_cut = any(r['budget_exhausted'] for r in results.values())
                completed = not budget_cut and not any(s['timed_out'] for r in results.values() for s in r['steps'])
                # Partial publication caused by our own budget cut is not a
                # toolchain discrepancy. Keep it as incomplete execution evidence.
                differences = [] if budget_cut else compare(results['baseline'], results['candidate'], tools)
                row = {'case': case['id'], 'config': case['config'], 'differences': differences,
                       'completed': completed, 'budget_interrupted': budget_cut,
                       **{side: summary(result) for side, result in results.items()}}
                if budget_cut or differences:
                    saved = save_pair(output, case, inputs, tools, results, differences, not budget_cut)
                    report['incomplete_bundles' if budget_cut else 'failures'].append(saved)
                report['results'].append(row)
                journal.write(json.dumps(row) + '\n')
                journal.flush()
                report['active'] = None
                write_json(output / 'active.json', {'case': None, 'completed_pairs': len(report['results'])})
                if (case['config'] == 'plain' and results['baseline']['accepted']
                        and not results['baseline']['errors']):
                    # check_products established presence and freshness. Retain
                    # actual inputs above before preparing later compare cases.
                    reference = results['baseline']['artifacts']['o.bin']
                    inputs['match.bin'] = reference
                    mismatch = bytearray(reference)
                    if mismatch:
                        mismatch[len(mismatch) // 2] ^= 255
                    else:
                        mismatch.append(255)
                    inputs['ref.bin'] = bytes(mismatch)
                    for name in ('ref.bin', 'match.bin'):
                        (output / 'cases' / case['id'] / name).write_bytes(inputs[name])
                if index % 100 == 0:
                    checkpoint()
                    print(f'{index + 1}/{len(workload)} comparisons; {len(report["failures"])} failures; '
                          f'{time.monotonic() - started:.1f}s', flush=True)
            report['state'] = 'finished'
    except (Exception, KeyboardInterrupt) as exc:
        report['state'] = 'interrupted' if isinstance(exc, KeyboardInterrupt) else 'error'
        report['error'] = f'{type(exc).__name__}: {exc}'
        (output / 'error.txt').write_text(traceback.format_exc())
    finally:
        checkpoint()
    if report['state'] == 'finished' and work.exists():
        shutil.rmtree(work)
    print(f'{"PASS" if report["passed"] else "FAIL"}: {len(report["results"])}/'
          f'{report.get("planned", {}).get("comparisons", "unknown")} comparisons, '
          f'{len(report["failures"])} failures, {len(report["omitted"])} incomplete/omitted, '
          f'{report["elapsed_seconds"]:.2f}s; {output / "report.json"}', flush=True)
    if report.get('error'):
        print(report['error'], file=sys.stderr)
    for error in report['coverage_errors']:
        print(error, file=sys.stderr)
    return 0 if report['passed'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate-dir', type=Path, required=True)
    parser.add_argument('--baseline-dir', type=Path, help='default: build pinned release from local Git history')
    parser.add_argument('--cache', type=Path, default=default_cache())
    parser.add_argument('--output', type=Path, required=True, help='new directory for reports and inputs')
    parser.add_argument('--seed', type=int, default=20261027)
    parser.add_argument('--binary-cases', type=int, default=100)
    parser.add_argument('--link-cases', type=int, default=100)
    parser.add_argument('--timeout', type=float, default=5)
    parser.add_argument('--budget', type=float, default=300)
    args = parser.parse_args()
    if min(args.binary_cases, args.link_cases) < 0 or any(
            not math.isfinite(n) or n <= 0 for n in (args.timeout, args.budget)):
        parser.error('counts must be nonnegative and time limits finite and positive')
    return run(args)


if __name__ == '__main__':
    signal.signal(signal.SIGTERM, interrupted)
    sys.exit(main())
