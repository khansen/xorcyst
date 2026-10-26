#!/usr/bin/env python3
"""Build the frozen source projects without running asset converters.

This source-checkout tool uses the external frozen gate archive. It never builds in
an input tree. Both tools must be explicit; installed xasm/xlnk are not used.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

from freeze_baselines import digest, manifest, minimal_environment

CONVERTERS = ('xm2nes', 'xm2btn', 'packchr', 'packnam')


def tools_at(destination, assembler, linker):
    destination.mkdir(parents=True, exist_ok=False)
    result = {}
    for name, source in (('xasm', assembler), ('xlnk', linker)):
        shutil.copy2(source, destination/name)
        result[name] = digest(destination/name)
    for name in CONVERTERS:
        p = destination/name
        p.write_text('#!/bin/sh\necho "converter forbidden in XORcyst gate" >&2\nexit 99\n')
        p.chmod(0o755)
    return result


def environment_at(root, tools):
    home, temporary = root/'home', root/'tmp'
    home.mkdir(); temporary.mkdir()
    environment = minimal_environment(home, str(tools)+':/usr/bin:/bin:/usr/sbin:/sbin')
    environment['TMPDIR'] = str(temporary)
    return environment


def make_command(project, tools):
    command = ['/usr/bin/make', '-B', '-j1']
    for target in project['generated_targets']:
        command += ['-o', target]
    return command + [project['target'], 'XASM='+str(tools/'xasm')+' $(AFLAGS)',
                      'XLNK='+str(tools/'xlnk')+' $(LFLAGS)',
                      'XM2NES='+str(tools/'xm2nes'), 'XM2BTN='+str(tools/'xm2btn')]


def verify_project(gate, name, project, root, tools, environment):
    inputs = gate/'inputs'/name
    expected = json.loads((gate/'manifests'/f'{name}.json').read_text())
    if manifest(inputs) != expected:
        raise ValueError('frozen input manifest changed: '+name)
    source = root/'source'
    shutil.copytree(inputs, source)
    cwd = source/project['build_directory']
    command = make_command(project, tools)
    run = subprocess.run(command, cwd=cwd, env=environment, capture_output=True)
    (root/'stdout.log').write_bytes(run.stdout)
    (root/'stderr.log').write_bytes(run.stderr)
    if run.returncode:
        raise ValueError(f'{name}: build failed ({run.returncode}); see {root}')
    product = cwd/project['product']
    actual = digest(product)
    if actual != project['expected_rom']:
        raise ValueError(f'{name}: ROM mismatch: {actual}')
    commands = {tool: [] for tool in ('xasm', 'xlnk')}
    for line in run.stdout.decode().splitlines():
        for tool in commands:
            if line.startswith(str(tools/tool)+' ') or line.startswith(tool+' '):
                argv = shlex.split(line)
                argv[0] = str(tools/tool)
                commands[tool].append(argv)
    if not commands['xasm'] or len(commands['xlnk']) != 1:
        raise ValueError(f'{name}: cannot identify complete assembler/linker command lists')
    # Outputs may be added to the copy, but no captured source or asset changes.
    if any(digest(source/path) != value for path, value in expected.items()):
        raise ValueError('build changed a frozen input in its copy: '+name)
    if manifest(inputs) != expected:
        raise ValueError('original frozen inputs changed: '+name)
    return dict(project=project, product=actual, command=command, commands=commands,
                cwd=str(cwd), source=str(source), input_manifest=expected,
                stderr=run.stderr.decode(), source_files=len(expected))


def capture(args):
    args.output.mkdir(parents=True, exist_ok=False)
    tools = args.output/'tools'
    hashes = tools_at(tools, args.assembler.resolve(), args.linker.resolve())
    environment = environment_at(args.output, tools)
    gate = args.gate.resolve()
    report = json.loads((gate/'gate.json').read_text())
    result = dict(gate=str(gate), gate_manifest=digest(gate/'gate.json'), tools=hashes,
                  environment=environment, projects={})
    for name, project in report['projects'].items():
        if args.project and name not in args.project:
            continue
        root = args.output/name
        root.mkdir()
        result['projects'][name] = verify_project(gate, name, project, root, tools, environment)
        (args.output/'results.json').write_text(json.dumps(result, indent=2)+'\n')
        print(name, 'ROM exact', flush=True)
    if not result['projects']:
        raise ValueError('no selected projects')
    for name, expected in hashes.items():
        if digest(tools/name) != expected:
            raise ValueError('tool changed during build: '+name)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gate', type=Path, required=True)
    parser.add_argument('--assembler', type=Path, required=True)
    parser.add_argument('--linker', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--project', action='append')
    args = parser.parse_args()
    args.output = args.output.resolve()
    capture(args)


if __name__ == '__main__':
    main()
