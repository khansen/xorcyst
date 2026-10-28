#!/usr/bin/env python3
"""Replay or reduce a saved differential failure without changing the original bundle."""
import argparse
import json
import math
from pathlib import Path
import shutil
import signal
import sys
import time

from common import compare, execute, interrupted, save_raw, tool_identity


def reduce_lines(inputs, predicate, attempts=200):
    """Bounded ddmin across source units and linker script; keep auxiliary bytes."""
    result = dict(inputs)
    used = 0
    for name in sorted(inputs):
        if not name.endswith(('.asm', '.s')):
            continue
        lines = result[name].splitlines(keepends=True)
        divisions = 2
        while lines and used < attempts:
            width = max(1, (len(lines) + divisions - 1) // divisions)
            changed = False
            for offset in range(0, len(lines), width):
                trial = lines[:offset] + lines[offset + width:]
                used += 1
                if predicate({**result, name: b''.join(trial)}):
                    lines = trial
                    result[name] = b''.join(lines)
                    divisions = max(2, divisions - 1)
                    changed = True
                    break
                if used >= attempts:
                    break
            if not changed:
                if width == 1:
                    break
                divisions = min(len(lines), divisions * 2)
    return result, used


def difference_classes(differences):
    # Keep every typed path/error class. Row-count values can shrink during
    # reduction, but do not collapse paths or drop a secondary byte regression.
    import re
    return frozenset(re.sub(r': row counts \d+ != \d+$', ': row counts differ', item)
                     for item in differences)


def signature(results, differences):
    # Keep all failure classes and both acceptance/exit patterns. A bytes
    # regression must not reduce into two matching parser failures or timeouts.
    return (difference_classes(differences),
            tuple((side, results[side]['accepted'],
                   tuple((s['returncode'], s['timed_out'], s.get('budget_exhausted', False))
                         for s in results[side]['steps']))
                  for side in ('baseline', 'candidate')))


def replay(args):
    bundle = args.bundle.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    case = json.loads((bundle / 'case.json').read_text())
    if (bundle / 'completion.json').exists():
        if not json.loads((bundle / 'completion.json').read_text())['complete']:
            message = 'this is a budget-interrupted pair, not a recorded regression'
            (output / 'replay.json').write_text(json.dumps({
                'reproduced': False, 'complete': False, 'error': message}, indent=2) + '\n')
            print(message)
            return 2
    recorded_differences = json.loads((bundle / 'differences.json').read_text())
    recorded_results = {side: json.loads((bundle / side / 'result.json').read_text())
                        for side in ('baseline', 'candidate')}
    recorded = json.loads((bundle / 'tools.json').read_text())
    tools = {}
    for side in ('baseline', 'candidate'):
        directory = getattr(args, side + '_dir')
        tools[side] = {}
        for name in ('xasm', 'xlnk'):
            path = directory / name if directory else Path(recorded[side][name]['path'])
            tools[side][name] = tool_identity(path)
            if not directory and tools[side][name]['sha256'] != recorded[side][name]['sha256']:
                raise ValueError(f'{side} {name} changed; specify an explicit replacement directory')
    inputs = {str(p.relative_to(bundle / 'inputs')): p.read_bytes()
              for p in (bundle / 'inputs').rglob('*') if p.is_file()}
    deadline = time.monotonic() + args.budget
    work = output / 'work'

    def pair(data):
        results = {side: execute(case, data, tools[side], work, args.timeout, deadline)
                   for side in ('baseline', 'candidate')}
        return results, compare(results['baseline'], results['candidate'], tools)

    results, differences = pair(inputs)
    complete = not any(result.get('budget_exhausted') for result in results.values())
    original_signature = signature(recorded_results, recorded_differences)
    reproduced = complete and signature(results, differences) == original_signature
    attempts = 0
    if args.reduce and reproduced:
        best = [results, differences]
        def predicate(data):
            if time.monotonic() >= deadline:
                return False
            trial, delta = pair(data)
            if not any(r.get('budget_exhausted') for r in trial.values()) and signature(trial, delta) == original_signature:
                best[:] = [trial, delta]
                return True
            return False
        inputs, attempts = reduce_lines(inputs, predicate, args.attempts)
        results, differences = best
    (output / 'case.json').write_text(json.dumps(case, indent=2) + '\n')
    (output / 'tools.json').write_text(json.dumps(tools, indent=2) + '\n')
    (output / 'differences.json').write_text(json.dumps(differences, indent=2) + '\n')
    (output / 'replay.json').write_text(json.dumps({
        'reproduced': reproduced, 'complete': complete, 'reduction_attempts': attempts,
        'expected_difference_classes': sorted(difference_classes(recorded_differences)),
        'missing_difference_classes': sorted(difference_classes(recorded_differences) - difference_classes(differences)),
        'new_difference_classes': sorted(difference_classes(differences) - difference_classes(recorded_differences)),
        'budget_exhausted': time.monotonic() >= deadline,
        'note': 'Bounded line reduction; syntactic minimality is not guaranteed.'}, indent=2) + '\n')
    for name, data in inputs.items():
        path = output / 'inputs' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    for side, result in results.items():
        save_raw(output / side, result)
    shutil.rmtree(work)
    if reproduced:
        print(f'Reproduced all {len(difference_classes(differences))} recorded difference classes; '
              f'{attempts} reduction attempts; {output}')
        return 1
    if not complete:
        print(f'Replay incomplete: wall-clock budget exhausted; {output}')
        return 2
    print(f'Recorded failure not reproduced; {len(differences)} current differences; {output}')
    return 2 if differences else 0


if __name__ == '__main__':
    signal.signal(signal.SIGTERM, interrupted)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline-dir', type=Path)
    parser.add_argument('--candidate-dir', type=Path)
    parser.add_argument('--reduce', action='store_true')
    parser.add_argument('--attempts', type=int, default=200)
    parser.add_argument('--budget', type=float, default=60)
    parser.add_argument('--timeout', type=float, default=5)
    args = parser.parse_args()
    if args.attempts < 0 or any(not math.isfinite(n) or n <= 0 for n in (args.budget, args.timeout)):
        parser.error('attempts must be nonnegative and time limits positive')
    try:
        sys.exit(replay(args))
    except KeyboardInterrupt:
        print('Replay interrupted', file=sys.stderr)
        sys.exit(130)
    except (ValueError, OSError, KeyError) as exc:
        print(f'Replay could not run: {exc}', file=sys.stderr)
        sys.exit(2)
