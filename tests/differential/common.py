"""Execution and comparison shared by the runner, replay, and reducer."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import time

from products import check_products, expected_products, product_metrics

OLD = b'previous differential output\n'
SANITIZER = re.compile(rb'(?:AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer):|runtime error:')


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def tool_identity(path):
    path = Path(path).resolve()
    result = subprocess.run([str(path), '--version'], capture_output=True, timeout=10, check=True)
    data = path.read_bytes()
    return {'path': str(path), 'sha256': sha256(data), 'size': len(data),
            'version': result.stdout.decode().strip()}


def environment(work):
    # No inherited XASM, DYLD/LD_PRELOAD, generator switches or user config.
    return {'PATH': '/usr/bin:/bin', 'HOME': str(work), 'TMPDIR': str(work),
            'LC_ALL': 'C', 'TZ': 'UTC'}


def stop_process(proc):
    """Kill the invocation's process group, tolerating exit/reap races on macOS."""
    try:
        os.killpg(proc.pid, signal.SIGKILL)
    except (ProcessLookupError, PermissionError):
        # Darwin may report EPERM for an already exited, unreaped group leader.
        # Poll/reap it; if it is still alive, terminate the owned child directly.
        if proc.poll() is None:
            try:
                proc.kill()
            except ProcessLookupError:
                pass


def invoke(tool, args, work, timeout, budget_limited=False):
    start = time.monotonic()
    command = [tool, *args]
    env = environment(work)
    proc = subprocess.Popen(command, cwd=work, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, start_new_session=True)
    timed_out = budget_exhausted = False
    try:
        stdout, stderr = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = not budget_limited
        budget_exhausted = budget_limited
        stop_process(proc)
        stdout, stderr = proc.communicate()
    except BaseException:
        stop_process(proc)
        proc.communicate()
        raise
    return {'command': command, 'cwd': str(work), 'environment': env,
            'returncode': proc.returncode, 'stdout': stdout,
            'stderr': stderr, 'timed_out': timed_out, 'budget_exhausted': budget_exhausted,
            'elapsed': time.monotonic() - start}


def input_snapshot(path):
    if path.is_symlink():
        return {'kind': 'symlink', 'target': os.readlink(path)}, None
    if not path.exists():
        return {'kind': 'missing'}, None
    if not path.is_file():
        return {'kind': 'non-file'}, None
    data = path.read_bytes()
    return {'kind': 'file', 'size': len(data), 'sha256': sha256(data)}, data


def execute(case, inputs, tools, work, timeout, deadline):
    # Keep the directory inode stable. Rapidly deleting/recreating a process's
    # cwd can leave stale identities on shared filesystems (e.g. virtiofs).
    # All contents still reset before every invocation/toolchain.
    work.mkdir(exist_ok=True)
    for path in work.iterdir():
        if path.is_dir() and not path.is_symlink():
            shutil.rmtree(path)
        else:
            path.unlink()
    for name, data in inputs.items():
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    steps, errors, input_changes = [], [], []
    changed_inputs = {}
    original_inputs = {name: {'kind': 'file', 'size': len(data), 'sha256': sha256(data)}
                       for name, data in inputs.items()}
    budget_exhausted = False
    commands = []
    args = case['args']
    if case['kind'] == 'binary':
        commands.append(('xasm', ['--pure-binary', 'p.asm', '-o', 'o.bin', *args]))
        if case['existing']:
            for name in expected_products(args):
                (work / name).write_bytes(OLD)
    else:
        for name in sorted(inputs):
            if name.endswith('.asm'):
                commands.append(('xasm', [name, '-o', Path(name).with_suffix('.o').name]))
        commands.append(('xlnk', ['link.s']))
        if case['existing']:
            (work / 'out.rom').write_bytes(OLD)
    for tool, argv in commands:
        if tool == 'xlnk' and any(s['returncode'] != 0 for s in steps):
            break
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            budget_exhausted = True
            break
        protected_inputs = dict(original_inputs)
        if tool == 'xlnk':
            # Assembled objects become inputs to the linker. Their encoding may
            # differ between toolchains, but neither linker may modify its own.
            protected_inputs.update({path.name: input_snapshot(path)[0]
                                     for path in work.glob('*.o')})
        step = invoke(tools[tool]['path'], argv, work, min(timeout, remaining), remaining < timeout)
        steps.append(step)
        for name, before in protected_inputs.items():
            try:
                after, data = input_snapshot(work / name)
            except OSError as exc:
                after, data = {'kind': 'unreadable', 'error': str(exc)}, None
            if after != before:
                errors.append('input changed: ' + name)
                input_changes.append({'stage': len(steps) - 1, 'path': name,
                                      'before': before, 'after': after})
                if data is not None:
                    changed_inputs[name] = data
        if step['budget_exhausted']:
            budget_exhausted = True
            break
        if step['timed_out']:
            errors.append('invocation timed out')
            break
        if step['returncode'] < 0:
            errors.append(f'invocation terminated by signal {-step["returncode"]}')
        if SANITIZER.search(step['stderr']):
            errors.append('sanitizer report')
        if step['returncode'] == 0 and tool == 'xasm' and case['kind'] == 'link':
            if not (work / argv[-1]).is_file():
                errors.append(f'missing assembled object: {argv[-1]}')
    artifacts = {str(p.relative_to(work)): p.read_bytes() for p in work.rglob('*')
                 if p.is_file() and str(p.relative_to(work)) not in inputs}
    accepted = (not budget_exhausted and len(steps) == len(commands) and all(not s['timed_out'] for s in steps)
                and steps[-1]['returncode'] in ((0, 4, 5) if case['kind'] == 'binary' else (0,)))
    if accepted and case['existing']:
        expected = expected_products(args) if case['kind'] == 'binary' else {'out.rom'}
        errors += ['accepted run left stale product: ' + name for name in sorted(expected)
                   if artifacts.get(name) == OLD]
    if case['kind'] == 'binary':
        errors += check_products(artifacts, args, accepted)
    elif accepted and 'out.rom' not in artifacts:
        errors.append('missing linked ROM')
    if accepted and case['kind'] == 'binary' and 'o.bin' in artifacts:
        reference = next((arg.split('=', 1)[1] for arg in args if arg.startswith('--compare=')), None)
        if reference in inputs:
            expected_status = 0 if artifacts['o.bin'] == inputs[reference] else 5
            if steps[-1]['returncode'] != expected_status:
                errors.append('comparison status disagrees with emitted/reference bytes')
    content = {}
    if accepted:
        for name, data in artifacts.items():
            try:
                content[name] = product_metrics(name, data)
            except (ValueError, KeyError, TypeError, IndexError, AttributeError) as exc:
                errors.append(f'{name}: invalid product content: {exc}')
    if not budget_exhausted and case.get('expected_hex') is not None and case['config'] in ('plain', 'link'):
        product = 'o.bin' if case['kind'] == 'binary' else 'out.rom'
        if not accepted or artifacts.get(product) != bytes.fromhex(case['expected_hex']):
            errors.append('control did not emit its expected bytes')
    if case.get('expected_instructions') is not None and case['config'] == 'all' and accepted:
        try:
            if len(json.loads(artifacts['r.json'])['records']) != case['expected_instructions']:
                errors.append('control instruction count changed')
        except (ValueError, KeyError, TypeError):
            errors.append('control instruction records missing or malformed')
    if not budget_exhausted and case.get('must_accept') and not accepted:
        errors.append('positive control rejected')
    if not budget_exhausted and case.get('must_reject') and (accepted or not steps or not steps[-1]['stderr']):
        errors.append('negative control needs a located rejection')
    if not budget_exhausted and case.get('must_reject') and artifacts.get('o.bin') != (OLD if case['existing'] else None):
        errors.append('rejected control did not preserve the earlier binary')
    return {'steps': steps, 'artifacts': artifacts, 'errors': sorted(set(errors)), 'accepted': accepted,
            'budget_exhausted': budget_exhausted, 'input_changes': input_changes,
            'changed_inputs': changed_inputs, 'content': content}


def normalize(name, data, tool):
    """Only documented, validated producer identity and the xref clock vary."""
    if name.endswith('.ndjson'):
        return [json.loads(line) for line in data.splitlines()]
    if not name.endswith('.json'):
        return data
    doc = json.loads(data)
    if name in ('x.json', 'd.json') and not isinstance(doc, dict):
        raise ValueError('expected a JSON object')
    if name == 'x.json' and 'build' in doc and 'timestamp_utc' in doc['build']:
        timestamp = doc['build']['timestamp_utc']
        if not isinstance(timestamp, str) or not re.fullmatch(r'\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ', timestamp):
            raise ValueError('invalid xref timestamp')
        doc['build']['timestamp_utc'] = '<clock>'
    if name == 'd.json':
        invocation = doc['invocation']
        if invocation['argv'][0] != tool['path']:
            raise ValueError('manifest invocation does not identify the running tool')
        invocation['argv'][0] = '<validated producer>'
        producers = [row for row in doc['inputs'] if 'producer' in row['roles']]
        if len(producers) != 1:
            raise ValueError('manifest must contain exactly one producer')
        row = producers[0]
        if (row['path'] != tool['path'] or row['sha256'] != tool['sha256'] or row['size'] != tool['size']):
            raise ValueError('manifest producer identity is incorrect')
        row.update(path='<validated producer>', sha256='<validated hash>', size=0)
        if doc['producer_version'] != tool['version']:
            raise ValueError('manifest producer version disagrees with --version')
        doc['producer_version'] = '<validated version>'
    return doc


def structural_diff(a, b, path='$', limit=12):
    """Bounded, typed JSON differences; absent keys are different from null."""
    result = []
    def walk(left, right, where):
        if len(result) >= limit:
            return
        if type(left) is not type(right):
            result.append(where + ': types differ')
        elif isinstance(left, dict):
            for key in sorted(left.keys() | right.keys()):
                if key not in left or key not in right:
                    result.append(where + '.' + key + ': missing field')
                else:
                    walk(left[key], right[key], where + '.' + key)
                if len(result) >= limit:
                    break
        elif isinstance(left, list):
            if len(left) != len(right):
                result.append(where + f': row counts {len(left)} != {len(right)}')
            for i, (x, y) in enumerate(zip(left, right)):
                walk(x, y, where + f'[{i}]')
                if len(result) >= limit:
                    break
        elif left != right:
            result.append(where + ': values differ')
    walk(a, b, path)
    return result[:limit]


def compare(a, b, tools):
    differences = []
    for side, result in (('baseline', a), ('candidate', b)):
        differences += [side + ': ' + error for error in result['errors']]
    if len(a['steps']) != len(b['steps']):
        differences.append('stage count differs')
    for i, (x, y) in enumerate(zip(a['steps'], b['steps'])):
        for key in ('returncode', 'stdout', 'stderr', 'timed_out'):
            if x[key] != y[key]:
                differences.append(f'stage {i}: {key} differs')
    for name in sorted(a['artifacts'].keys() | b['artifacts'].keys()):
        if name not in a['artifacts'] or name not in b['artifacts']:
            differences.append(name + ': artifact presence differs')
            continue
        # Each version consumes its own objects. Object encoding is not the
        # oracle; final linked bytes, object presence, and diagnostics are.
        if name.endswith('.o'):
            continue
        values = []
        for side, result in (('baseline', a), ('candidate', b)):
            data = result['artifacts'][name]
            try:
                # An intentionally preserved pre-existing file is not JSON.
                values.append(data if data == OLD else normalize(name, data, tools[side]['xasm']))
            except (ValueError, KeyError, TypeError, IndexError, AttributeError) as exc:
                differences.append(side + ': ' + name + ': ' + str(exc))
                values.append(data)
        differences.extend(structural_diff(*values, path=name))
    return differences


def summary(result):
    return {'accepted': result['accepted'], 'errors': result['errors'],
            'budget_exhausted': result.get('budget_exhausted', False),
            'input_changes': result.get('input_changes', []),
            'content': result.get('content', {}),
            'artifacts': {name: {'sha256': sha256(data), 'size': len(data)}
                          for name, data in sorted(result['artifacts'].items())},
            'steps': [{key: value for key, value in step.items() if key not in ('stdout', 'stderr')}
                      | {'stdout_sha256': sha256(step['stdout']), 'stderr_sha256': sha256(step['stderr']),
                         'warnings': len(re.findall(rb'\bwarning(?: W\d+)?:', step['stderr'], re.I)),
                         'diagnostics': step['stderr'].decode(errors='replace')}
                      for step in result['steps']]}


def save_raw(destination, result):
    destination.mkdir(parents=True)
    (destination / 'result.json').write_text(json.dumps(summary(result), indent=2) + '\n')
    for i, step in enumerate(result['steps']):
        for stream in ('stdout', 'stderr'):
            (destination / f'{i}.{stream}').write_bytes(step[stream])
    for name, data in result['artifacts'].items():
        path = destination / 'artifacts' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    for name, data in result.get('changed_inputs', {}).items():
        path = destination / 'changed-inputs' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
