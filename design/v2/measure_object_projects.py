#!/usr/bin/env python3
"""Paired I4 assembly, linking and complete-build measurements.

Inputs are two successful check_object_projects.py captures made with matched
compiler settings. This runs only in fresh hard-linked copies of those scratch
builds, with frozen assets and converter traps. See PERFORMANCE.md.
"""
import argparse
import json
import math
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import sys
import time

from check_object_projects import digest, environment_at, make_command, tools_at
sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tests'))
from perf_ir_frontend import interval


def object_output(command):
    for i, argument in enumerate(command[1:], 1):
        if argument in ('-o', '--output'):
            return Path(command[i+1])
        if argument.startswith('--output='):
            return Path(argument.split('=', 1)[1])
        if argument.startswith('-o'):
            return Path(argument[2:])
    sources = [p for p in command[1:] if p.lower().endswith('.asm')]
    if len(sources) != 1:
        raise ValueError('cannot determine object destination: '+repr(command))
    return Path(sources[0]).with_suffix('.o')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--project', action='append')
    parser.add_argument('--stage', choices=('assembly', 'link', 'build'), action='append')
    parser.add_argument('--pairs', type=int)
    parser.add_argument('--survey', action='store_true',
                        help='bounded absolute-cost survey: five pairs, one invocation each; not a formal gate')
    parser.add_argument('--reverse', action='store_true')
    parser.add_argument('--batch-from', type=Path,
                        help='reuse fixed batch counts from an earlier matched session')
    args = parser.parse_args()
    if args.pairs is None:
        args.pairs = 5 if args.survey else 15
    if args.pairs < (5 if args.survey else 15):
        parser.error('at least five survey pairs or 15 formal pairs are required')
    if args.survey and args.batch_from:
        parser.error('--survey uses one invocation per sample, not --batch-from')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    captures = {key: json.loads((path/'results.json').read_text())
                for key, path in (('v1', args.baseline), ('v2', args.candidate))}
    if captures['v1']['gate_manifest'] != captures['v2']['gate_manifest']:
        raise ValueError('captures use different gate manifests')
    toolpaths = {}
    environments = {}
    for key, capture in captures.items():
        base = output/key; base.mkdir()
        tools = base/'tools'; toolpaths[key] = tools
        source = (args.baseline if key == 'v1' else args.candidate).resolve()/'tools'
        actual = tools_at(tools, source/'xasm', source/'xlnk')
        if actual != capture['tools']:
            raise ValueError('captured tools changed: '+key)
        environments[key] = environment_at(base, tools)
    report = dict(platform=platform.platform(), rss_units='bytes',
                  harness=digest(Path(__file__)),
                  captures={k:str((args.baseline if k=='v1' else args.candidate).resolve()) for k in captures},
                  tools={k:c['tools'] for k,c in captures.items()}, environments=environments,
                  gate_manifest=captures['v1']['gate_manifest'], bootstrap_seed=20261024,
                  bootstrap_resamples=10000, timing='sum of per-invocation process-tree intervals',
                  copies='fresh per assembly/build; private link inputs per batch with product removed per invocation',
                  measurement_kind='absolute-cost survey' if args.survey else 'formal paired session',
                  projects={})
    prior = None
    if args.batch_from:
        prior = json.loads(args.batch_from.read_text())
        if prior['tools'] != report['tools'] or prior['gate_manifest'] != report['gate_manifest']:
            raise ValueError('batch source uses different tools or fixtures')
        report['batch_source'] = str(args.batch_from.resolve())

    def save():
        (output/'results.json').write_text(json.dumps(report, indent=2)+'\n')

    for name, baseline in captures['v1']['projects'].items():
        if args.project and name not in args.project: continue
        versions = {k:c['projects'][name] for k,c in captures.items()}
        if any(v['product'] != baseline['product'] or v['input_manifest'] != baseline['input_manifest']
               for v in versions.values()):
            raise ValueError('project capture mismatch: '+name)
        root = output/name; root.mkdir()
        project_record = report['projects'][name] = dict(expected=baseline['product'], stages={})
        object_inputs = project_record['objects'] = {}
        for key, version in versions.items():
            object_inputs[key] = {}
            for command in version['commands']['xasm']:
                path = object_output(command)
                if path.is_absolute() or '..' in path.parts:
                    raise ValueError('unsafe object destination: '+str(path))
                object_inputs[key][str(path)] = digest(Path(version['cwd'])/path)
        for stage in args.stage or ('assembly', 'link', 'build'):
            directory = root/stage; directory.mkdir()
            logs = directory/'logs'; logs.mkdir()
            record = project_record['stages'][stage] = dict(samples=[])
            diagnostics = None

            def batch(key, label, repetitions):
                nonlocal diagnostics
                version = versions[key]
                project = version['project']
                commands = version['commands']
                # Link input objects are immutable. Reuse one private input
                # copy within a batch, but unlink the product before EVERY
                # invocation. No prior ROM is reused. This avoids hundreds of
                # redundant input-tree copies triggering filesystem indexing.
                # Assembly/build repetitions each start in their own clean copy.
                clone = None
                elapsed, cpu, peak, command_count = 0.0, 0.0, 0, 0
                path = logs/f'{key}-{label}.stderr'
                with path.open('wb') as handle:
                    for n in range(repetitions):
                        if stage != 'link' or clone is None:
                            clone = directory/f'{key}-{label}-{n}'
                            shutil.copytree(version['source'], clone, copy_function=os.link)
                            cwd = clone/project['build_directory']
                            product = cwd/project['product']
                        # -o opens an initial output; it does not override a
                        # script's output{} command. Keep scripts/argv unchanged.
                        # Unlink first: v1 writes directly through existing files.
                        product.unlink(missing_ok=True)
                        if stage != 'link':
                            for command in commands['xasm']:
                                object_path = object_output(command)
                                if object_path.is_absolute() or '..' in object_path.parts:
                                    raise ValueError('unsafe object destination: '+str(object_path))
                                (cwd/object_path).unlink(missing_ok=True)
                        if stage == 'build':
                            selected = [make_command(project, toolpaths[key])]
                        else:
                            tool = 'xasm' if stage == 'assembly' else 'xlnk'
                            selected = [[str(toolpaths[key]/tool), *command[1:]]
                                        for command in commands[tool]]
                            if stage == 'link' and len(selected) != 1:
                                raise ValueError('link timing requires one ROM-producing invocation')
                        start = handle.tell()
                        statuses = []
                        elapsed_start = time.perf_counter()
                        for command in selected:
                            child = subprocess.Popen(command, cwd=cwd, env=environments[key],
                                                     stdout=subprocess.DEVNULL, stderr=handle)
                            _, status, usage = os.wait4(child.pid, 0)
                            child.returncode = os.waitstatus_to_exitcode(status)
                            statuses.append(child.returncode)
                            cpu += usage.ru_utime + usage.ru_stime
                            peak = max(peak, usage.ru_maxrss*(1 if sys.platform=='darwin' else 1024))
                        elapsed += time.perf_counter()-elapsed_start
                        command_count += len(selected)
                        if any(statuses):
                            raise ValueError(f'failed measurement: {statuses}: {path}')
                        end = handle.tell()
                        with path.open('rb') as reader:
                            reader.seek(start)
                            stderr = reader.read(end-start)
                        if diagnostics is None:
                            diagnostics = stderr; (directory/'expected.stderr').write_bytes(stderr)
                        if stderr != diagnostics:
                            raise ValueError('diagnostic mismatch: '+str(clone))
                        if stage == 'assembly':
                            command = [str(toolpaths[key]/'xlnk'), *commands['xlnk'][0][1:]]
                            check = subprocess.run(command,cwd=cwd,env=environments[key],capture_output=True)
                            if check.returncode: raise ValueError('assembly output did not link: '+str(clone))
                        if digest(product) != baseline['product']:
                            raise ValueError('ROM mismatch: '+str(product))
                        if stage != 'link' or n == repetitions-1:
                            for input_name,value in version['input_manifest'].items():
                                input_path = clone/input_name
                                if digest(input_path) != value: raise ValueError('input changed: '+str(input_path))
                            if stage == 'link':
                                for object_path, expected in object_inputs[key].items():
                                    if digest(cwd/object_path) != expected:
                                        raise ValueError('link changed object: '+object_path)
                            shutil.rmtree(clone)
                return dict(elapsed=elapsed,cpu=cpu,peak_rss=peak,repetitions=repetitions,
                            command_count=command_count)

            warmups = {key:[batch(key,f'warm{i}',1) for i in range(2)] for key in versions}
            repetitions = max(1,math.ceil(2.5/min(s['elapsed'] for samples in warmups.values() for s in samples)))
            if args.survey:
                repetitions = 1
            if prior is not None:
                repetitions = prior['projects'][name]['stages'][stage]['batch']
                if not isinstance(repetitions, int) or repetitions < 1:
                    raise ValueError('invalid fixed batch count in prior session')
            record.update(warmups=warmups,batch=repetitions,
                          commands={k:v['commands'] if stage != 'build' else v['command'] for k,v in versions.items()})
            save()
            for pair in range(args.pairs):
                order = ['v2','v1'] if (pair+args.reverse)%2 else ['v1','v2']
                sample = dict(order=order)
                for key in order: sample[key] = batch(key,str(pair),repetitions)
                record['samples'].append(sample); save()
                print(name,stage,pair+1,'wall ratio',sample['v2']['elapsed']/sample['v1']['elapsed'],flush=True)
            summarize = (lambda ratios: dict(median=statistics.median(ratios))) if args.survey else interval
            record['statistics'] = {metric:summarize([s['v2'][metric]/s['v1'][metric] for s in record['samples']])
                                    for metric in ('elapsed','cpu')}
            record['absolute_ms'] = {
                metric: dict(
                    median={key: statistics.median(s[key][metric]/s[key]['repetitions']*1000
                                                   for s in record['samples'])
                            for key in versions},
                    median_paired_delta=statistics.median(
                        (s['v2'][metric]/s['v2']['repetitions']-
                         s['v1'][metric]/s['v1']['repetitions'])*1000
                        for s in record['samples']))
                for metric in ('elapsed', 'cpu')}
            peaks = {k:max(s[k]['peak_rss'] for s in record['samples']) for k in versions}
            ratios = [s['v2']['peak_rss']/s['v1']['peak_rss'] for s in record['samples']]
            record['memory'] = dict(maximum_bytes=peaks,maximum_ratio=peaks['v2']/peaks['v1'],
                                    median_paired_ratio=statistics.median(ratios),worst_paired_ratio=max(ratios),
                                    limit_exclusive=1.30,passes=max(ratios)<1.30 and peaks['v2']/peaks['v1']<1.30)
            save()
    for key, capture in captures.items():
        for tool, expected in capture['tools'].items():
            if digest(toolpaths[key]/tool) != expected: raise ValueError('tool changed during timing')
        for project in report['projects']:
            original = capture['projects'][project]
            for path, value in original['input_manifest'].items():
                if digest(Path(original['source'])/path) != value: raise ValueError('captured source changed')
            for path, value in report['projects'][project]['objects'][key].items():
                if digest(Path(original['cwd'])/path) != value: raise ValueError('captured object changed')
    save()


if __name__ == '__main__':
    main()
