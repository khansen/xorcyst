#!/usr/bin/env python3
"""Validate the differential oracle with deliberate defects, without a reference build."""
import copy
from collections import Counter
from contextlib import redirect_stdout, redirect_stderr
import io
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import tarfile
import tempfile
import textwrap
import time
import unittest
from unittest.mock import patch, Mock
from argparse import Namespace

sys.path.insert(0, str(Path(__file__).with_name('differential')))
from cases import controls, generate
from common import OLD, compare, execute, invoke, normalize, structural_diff
from products import CONFIGS, check_products, product_metrics
from replay import reduce_lines, signature, replay
from build_reference import build_reference, default_cache, private_cache, require_owned, run_build_command
from reporting import rejection_categories, zero_success_errors
import run as runner


IDENTITY = {'path': '/test/tool', 'sha256': 'a' * 64, 'size': 10, 'version': 'xasm 2.0.0'}
TOOLS = {side: {'xasm': IDENTITY, 'xlnk': IDENTITY} for side in ('baseline', 'candidate')}


def result(data=b'\xea'):
    return {'steps': [{'command': ['/test/tool'], 'returncode': 0, 'stdout': b'', 'stderr': b'',
                       'timed_out': False, 'elapsed': 0}],
            'artifacts': {'o.bin': data}, 'errors': [], 'accepted': True}


class Differential(unittest.TestCase):
    def test_byte_diagnostic_status_and_missing_artifact_mutants_fail(self):
        base = result()
        self.assertEqual(compare(base, copy.deepcopy(base), TOOLS), [])
        mutations = [lambda r: r['artifacts'].update({'o.bin': b'\x60'}),
                     lambda r: r['artifacts'].clear(),
                     lambda r: r['steps'][0].update(stderr=b'p.asm:3: warning: new warning\n'),
                     lambda r: r['steps'][0].update(stdout=b'unexpected output\n'),
                     lambda r: r['steps'][0].update(returncode=1),
                     lambda r: r['steps'].append(copy.deepcopy(r['steps'][0]))]
        for mutate in mutations:
            candidate = copy.deepcopy(base)
            mutate(candidate)
            with self.subTest(candidate=candidate):
                self.assertTrue(compare(base, candidate, TOOLS))

    def test_json_normalization_keeps_locations_rows_and_missing_fields(self):
        base = result()
        document = {'build': {'timestamp_utc': '2026-10-26T01:02:03Z', 'output_file': 'o.bin'},
                    'records': [{'file': 'p.asm', 'line': 7, 'value': None}]}
        base['artifacts']['x.json'] = json.dumps(document).encode()
        other = copy.deepcopy(document)
        other['build']['timestamp_utc'] = '2026-10-27T04:05:06Z'
        candidate = copy.deepcopy(base)
        candidate['artifacts']['x.json'] = json.dumps(other, sort_keys=True, indent=2).encode()
        self.assertFalse(compare(base, candidate, TOOLS))
        for mutate in (lambda d: d['records'].clear(), lambda d: d['records'][0].pop('value'),
                       lambda d: d['records'][0].update(line=8),
                       lambda d: d['build'].pop('timestamp_utc'),
                       lambda d: d['build'].update(output_file='wrong.bin')):
            broken = copy.deepcopy(other)
            mutate(broken)
            candidate['artifacts']['x.json'] = json.dumps(broken).encode()
            self.assertTrue(compare(base, candidate, TOOLS))
        self.assertTrue(structural_diff({'a': True}, {'a': 1}))
        a, b = result(), result()
        a['artifacts']['data.json'] = b'{"value": true}'
        b['artifacts']['data.json'] = b'{"value": 1}'
        self.assertTrue(compare(a, b, TOOLS))
        self.assertEqual(len(structural_diff({}, {str(i): i for i in range(100)}, limit=3)), 3)

    def test_producer_normalization_validates_identity_instead_of_dropping_it(self):
        document = {'producer_version': 'xasm 2.0.0', 'invocation': {'argv': ['/test/tool', 'p.asm']},
                    'inputs': [{'path': '/test/tool', 'sha256': 'a' * 64, 'size': 10,
                                'roles': ['producer']}]}
        self.assertEqual(normalize('d.json', json.dumps(document).encode(), IDENTITY)['inputs'][0]['roles'],
                         ['producer'])
        for field, value in (('sha256', 'b' * 64), ('path', '/wrong'), ('size', 11)):
            bad = copy.deepcopy(document)
            bad['inputs'][0][field] = value
            with self.assertRaises(ValueError):
                normalize('d.json', json.dumps(bad).encode(), IDENTITY)
        document['inputs'].clear()
        with self.assertRaises(ValueError):
            normalize('d.json', json.dumps(document).encode(), IDENTITY)

    def test_empty_or_incorrect_listing_cannot_pass_by_equality(self):
        args = ['--listing=l.json', '--listing-format=json']
        document = {'version': '2', 'binary_generated': True, 'records': []}
        artifacts = {'o.bin': b'\xea', 'l.json': json.dumps(document).encode()}
        self.assertTrue(check_products(artifacts, args, True))
        self.assertTrue(check_products({'o.bin': b'\xea'}, args, True))
        document['records'] = [{'emits_bytes': True, 'byte_count': 1, 'bytes_hex': ['EA'],
                                'fill_byte': None, 'output_offset_start': 0, 'output_offset_end': 0}]
        artifacts['l.json'] = json.dumps(document).encode()
        self.assertFalse(check_products(artifacts, args, True))
        artifacts['o.bin'] = b'\x60'
        self.assertTrue(check_products(artifacts, args, True))

    def test_reducer_keeps_predicate_and_auxiliary_files(self):
        inputs = {'p.asm': b'nop\nKEEP\nrts\n', 'second.asm': b'noise\n', 'blob.bin': b'\x00\xff'}
        reduced, count = reduce_lines(inputs, lambda files: b'KEEP' in files['p.asm'])
        self.assertEqual(reduced, {'p.asm': b'KEEP\n', 'second.asm': b'', 'blob.bin': b'\x00\xff'})
        self.assertGreater(count, 0)
        candidate = result(b'\x60')
        original = {'baseline': result(), 'candidate': candidate}
        changed = copy.deepcopy(original)
        changed['baseline']['steps'][0]['returncode'] = 1
        self.assertNotEqual(signature(original, ['bytes']), signature(changed, ['bytes']))

    def test_generation_is_seeded_and_ignores_old_environment_switches(self):
        first = generate(1234, 12, 5)
        old = dict(os.environ)
        try:
            os.environ.update(NOCHAR='1', NOSTRDATA='1', NOORGPC='1', NOUNDEFPROC='1', FWDSTR='0')
            self.assertEqual(first, generate(1234, 12, 5))
        finally:
            os.environ.clear(); os.environ.update(old)
        self.assertNotEqual(first, generate(1235, 12, 5))
        self.assertEqual(len(first), 17)

    def test_real_tools_accept_controls_and_agree_with_themselves(self):
        root = Path(__file__).resolve().parents[1]
        asm = Path(os.environ.get('XASM', root / 'xasm')).resolve()
        lnk = Path(os.environ.get('XLNK', root / 'xlnk')).resolve()
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory) / 'work'
            tools = {'xasm': {'path': str(asm)}, 'xlnk': {'path': str(lnk)}}
            for case, inputs in controls():
                if case['id'] == 'control-mirror':
                    config, args = next(iter(case['configs'].items()))
                else:
                    config, args = ('plain' if case['kind'] == 'binary' else 'link'), []
                case.update(config=config, args=args)
                a = execute(case, inputs, tools, work, 5, time.monotonic() + 30)
                inode = work.stat().st_ino
                (work / 'stale-product').write_bytes(b'old')
                b = execute(case, inputs, tools, work, 5, time.monotonic() + 30)
                with self.subTest(case=case['id']):
                    self.assertEqual(work.stat().st_ino, inode)
                    self.assertNotIn('stale-product', b['artifacts'])
                    self.assertFalse(a['errors'])
                    self.assertFalse(compare(a, b, {'baseline': tools, 'candidate': tools}))

    def test_timeout_is_a_failure_even_for_identical_tools(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / 'slow'
            script.write_text('#!/bin/sh\nsleep 5\n')
            script.chmod(0o755)
            tools = {name: {'path': str(script)} for name in ('xasm', 'xlnk')}
            case = {'kind': 'binary', 'config': 'plain', 'args': [], 'existing': False}
            a = execute(case, {'p.asm': b'nop\n'}, tools, root / 'work', .02, time.monotonic() + 1)
            self.assertTrue(a['steps'][0]['timed_out'])
            self.assertTrue(compare(a, a, {'baseline': tools, 'candidate': tools}))

    def test_cli_retains_mutant_bundles_and_reduces_a_byte_difference(self):
        root = Path(__file__).resolve().parents[1]
        asm = str(Path(os.environ.get('XASM', root / 'xasm')).resolve())
        lnk = str(Path(os.environ.get('XLNK', root / 'xlnk')).resolve())
        suite = Path(__file__).with_name('differential')
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            baseline = directory / 'baseline'
            baseline.mkdir()
            (baseline / 'xasm').symlink_to(asm)
            (baseline / 'xlnk').symlink_to(lnk)
            for mutation in ('bytes', 'diagnostic', 'missing'):
                candidate = directory / mutation
                candidate.mkdir()
                (candidate / 'xlnk').symlink_to(lnk)
                wrapper = candidate / 'xasm'
                wrapper.write_text(f'#!{sys.executable} -BI\n' +
                    'import subprocess,sys\nfrom pathlib import Path\n' +
                    f'r=subprocess.run([{asm!r},*sys.argv[1:]])\n' +
                    "if r.returncode == 0 and '-o' in sys.argv and Path('p.asm').exists() and 'lda #1' in Path('p.asm').read_text():\n" +
                    " p=Path(sys.argv[sys.argv.index('-o')+1])\n" +
                    {'bytes': " p.write_bytes(p.read_bytes()+b'X')\n",
                     'diagnostic': " sys.stderr.write('p.asm:1: warning: injected diagnostic\\n')\n",
                     'missing': " p.unlink()\n"}[mutation] +
                    'sys.exit(r.returncode)\n')
                wrapper.chmod(0o755)
                output = directory / ('report-' + mutation)
                run = subprocess.run([sys.executable, '-B', str(suite / 'run.py'),
                    '--baseline-dir', str(baseline), '--candidate-dir', str(candidate),
                    '--binary-cases', '0', '--link-cases', '0', '--output', str(output)],
                    capture_output=True, timeout=30)
                self.assertEqual(run.returncode, 1, run.stdout + run.stderr)
                report = json.loads((output / 'report.json').read_text())
                self.assertTrue(report['complete'])
                row = next(r for r in report['results'] if r['case'] == 'control-analysis' and r['config'] == 'plain')
                expected = {'bytes': 'o.bin: values differ', 'diagnostic': 'stage 0: stderr differs',
                            'missing': 'o.bin: artifact presence differs'}[mutation]
                self.assertIn(expected, row['differences'])
                bundle = output / 'failures' / 'control-analysis-plain'
                self.assertTrue((bundle / 'baseline' / '0.stderr').exists())
                if mutation == 'bytes':
                    reduced = directory / 'reduced'
                    replay = subprocess.run([sys.executable, '-B', str(suite / 'replay.py'), str(bundle),
                        '--output', str(reduced), '--reduce'], capture_output=True, timeout=30)
                    self.assertEqual(replay.returncode, 1, replay.stdout + replay.stderr)
                    source = (reduced / 'inputs' / 'p.asm').read_bytes()
                    self.assertIn(b'lda #1', source)
                    self.assertLess(len(source), len((bundle / 'inputs' / 'p.asm').read_bytes()))
                    self.assertIn(expected, json.loads((reduced / 'differences.json').read_text()))

    def test_deadline_reports_omitted_cases_and_never_passes(self):
        root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / 'report'
            tools = Path(os.environ.get('XASM', root / 'xasm')).resolve().parent
            run = subprocess.run([sys.executable, '-B', str(Path(__file__).with_name('differential') / 'run.py'),
                '--baseline-dir', str(tools), '--candidate-dir', str(tools), '--output', str(output),
                '--budget', '0.000001', '--binary-cases', '1', '--link-cases', '1'],
                capture_output=True, timeout=10)
            self.assertEqual(run.returncode, 1, run.stdout + run.stderr)
            report = json.loads((output / 'report.json').read_text())
            self.assertFalse(report['complete'])
            self.assertEqual(len(report['omitted']), report['planned']['comparisons'])
            self.assertEqual(report['programs']['binary']['counts']['completed'], 0)


class ReviewRegressions(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='xorcyst-review-tests-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        repo = Path(__file__).resolve().parents[1]
        self.actual = {'xasm': str(Path(os.environ.get('XASM', repo / 'xasm')).resolve()),
                       'xlnk': str(Path(os.environ.get('XLNK', repo / 'xlnk')).resolve())}
        self.baseline = self.root / 'baseline'
        self.baseline.mkdir()
        for name, path in self.actual.items():
            (self.baseline / name).symlink_to(path)
        self.number = 0

    def wait_for(self, predicate):
        deadline = time.monotonic() + 10
        while not predicate():
            if time.monotonic() >= deadline:
                self.fail('test process did not reach its synchronization point')
            time.sleep(0.01)

    def mutant(self, body, run_real=True):
        self.number += 1
        directory = self.root / f'mutant-{self.number}'
        directory.mkdir()
        (directory / 'xlnk').symlink_to(self.actual['xlnk'])
        script = directory / 'xasm'
        # One combined shebang option is portable to Linux as well as Darwin.
        script.write_text(f'#!{sys.executable} -BI\nimport subprocess,sys,os,time\nfrom pathlib import Path\n' +
            (f'r=subprocess.run([{self.actual["xasm"]!r},*sys.argv[1:]])\nstatus=r.returncode\n'
             if run_real else 'status=0\n') +
            (f'if "--version" in sys.argv:\n subprocess.run([{self.actual["xasm"]!r},"--version"])\n'
             if not run_real else '') +
            'if "-o" in sys.argv and status == 0:\n' +
            ''.join(' ' + line + '\n' for line in body.splitlines()) + 'sys.exit(status)\n')
        script.chmod(0o755)
        return directory

    def workload(self, candidate, configurations=None, source=b'.org $8000\nlda #1\nrts\n',
                 baseline=None, budget=10, timeout=2, existing=True, extra_inputs=None):
        self.number += 1
        output = self.root / f'results-{self.number}'
        case = {'id': 'review-control', 'kind': 'binary', 'generator': 'fixture', 'existing': existing,
                'configs': configurations or {'plain': []}}
        inputs = {'p.asm': source, **(extra_inputs or {})}
        args = Namespace(output=output, baseline_dir=baseline or self.baseline,
                         candidate_dir=candidate, cache=self.root / 'cache', seed=123,
                         binary_cases=0, link_cases=0, budget=budget, timeout=timeout)
        with patch.object(runner, 'controls', return_value=[(case, inputs)]), \
                patch.object(runner, 'generate', return_value=[]), \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            status = runner.run(args)
        return status, json.loads((output / 'report.json').read_text()), output

    def test_changed_inputs_fail_and_retain_bytes_and_hashes(self):
        candidate = self.mutant("for name in ('p.asm','ref.bin','match.bin','map.txt','blob.bin','hdr','link.s'):\n Path(name).write_bytes(b'clobbered')")
        status, report, output = self.workload(candidate, extra_inputs={
            'map.txt': b'A=1\n', 'blob.bin': b'\x01\x02', 'hdr': b'NES', 'link.s': b'unused'})
        self.assertEqual(status, 1)
        result = report['results'][0]['candidate']
        self.assertEqual({c['path'] for c in result['input_changes']},
                         {'p.asm', 'ref.bin', 'match.bin', 'map.txt', 'blob.bin', 'hdr', 'link.s'})
        self.assertTrue(all(c['before'] != c['after'] for c in result['input_changes']))
        bundle = output / report['failures'][0]
        self.assertEqual((bundle / 'candidate/changed-inputs/p.asm').read_bytes(), b'clobbered')
        self.assertIn(b'lda #1', (bundle / 'inputs/p.asm').read_bytes())

    def test_input_changes_are_checked_between_object_stages(self):
        candidate = self.mutant("Path('hdr').write_bytes(b'bad' if '0.asm' in sys.argv else b'original')")
        tools = {name: {'path': str(candidate / name)} for name in self.actual}
        case = {'kind': 'link', 'config': 'link', 'args': [], 'existing': False}
        result = execute(case, {'0.asm': b'nop\n', '1.asm': b'rts\n', 'hdr': b'original',
            'link.s': b'output{file=out.rom}\nlink{file=0.o}\nlink{file=1.o}\n'},
            tools, self.root / 'work', 2, time.monotonic() + 10)
        self.assertIn('input changed: hdr', result['errors'])
        self.assertEqual(result['input_changes'][0]['stage'], 0)
        self.assertEqual((self.root / 'work/hdr').read_bytes(), b'original')

    def test_linker_cannot_modify_its_generated_object_inputs(self):
        candidate = self.mutant('pass')
        linker = candidate / 'xlnk'
        linker.unlink()
        linker.write_text(f'#!{sys.executable} -BI\nimport subprocess,sys\nfrom pathlib import Path\n'
                          f'r=subprocess.run([{self.actual["xlnk"]!r},*sys.argv[1:]])\n'
                          "Path('0.o').write_bytes(b'clobbered')\nsys.exit(r.returncode)\n")
        linker.chmod(0o755)
        tools = {name: {'path': str(candidate / name)} for name in self.actual}
        case = {'kind': 'link', 'config': 'link', 'args': [], 'existing': False}
        result = execute(case, {'0.asm': b'nop\n', 'link.s': b'output{file=out.rom}\nlink{file=0.o}\n'},
                         tools, self.root / 'work', 2, time.monotonic() + 10)
        self.assertIn('input changed: 0.o', result['errors'])

    def test_accepted_stale_outputs_fail_even_when_both_sides_agree(self):
        candidate = self.mutant('pass', run_real=False)
        status, report, _ = self.workload(candidate, baseline=candidate)
        self.assertEqual(status, 1)
        self.assertTrue(report['complete'])
        for side in ('baseline', 'candidate'):
            self.assertIn('accepted run left stale product: o.bin', report['results'][0][side]['errors'])

    def test_malformed_xref_shapes_and_missing_baseline_binary_leave_reports(self):
        for value in ('[]', 'null'):
            with self.subTest(value=value):
                candidate = self.mutant(f"Path('x.json').write_text({value!r})")
                status, report, _ = self.workload(candidate, {'xref': ['--xref=x.json']})
                self.assertEqual(status, 1)
                self.assertEqual(report['state'], 'finished')
                self.assertTrue(report['results'][0]['candidate']['errors'])
        baseline = self.mutant("Path(sys.argv[sys.argv.index('-o')+1]).unlink()")
        status, report, _ = self.workload(self.baseline, baseline=baseline, existing=False)
        self.assertEqual(status, 1)
        self.assertEqual(report['state'], 'finished')
        self.assertIn('missing requested products: o.bin', report['results'][0]['baseline']['errors'])

    def test_reduction_preserves_all_differences_and_replay_rejects_a_substitute(self):
        body = "source=Path('p.asm').read_text()\nif 'rts' in source:\n sys.stderr.write('warning: mutant\\n')\nif 'lda #1' in source:\n p=Path(sys.argv[sys.argv.index('-o')+1]); p.write_bytes(p.read_bytes()+b'X')"
        candidate = self.mutant(body)
        status, report, output = self.workload(candidate)
        self.assertEqual(status, 1)
        bundle = output / report['failures'][0]
        args = Namespace(bundle=bundle, output=self.root / 'reduced', baseline_dir=None,
                         candidate_dir=None, budget=10, timeout=2, reduce=True, attempts=100)
        with redirect_stdout(io.StringIO()):
            self.assertEqual(replay(args), 1)
        source = (args.output / 'inputs/p.asm').read_bytes()
        self.assertIn(b'rts', source)
        self.assertIn(b'lda #1', source)
        self.assertEqual(set(json.loads((args.output / 'differences.json').read_text())),
                         {'stage 0: stderr differs', 'o.bin: values differ'})
        args.candidate_dir = self.mutant("if 'rts' in Path('p.asm').read_text():\n sys.stderr.write('warning: mutant\\n')")
        args.output = self.root / 'substitute'
        with redirect_stdout(io.StringIO()):
            self.assertEqual(replay(args), 2)
        detail = json.loads((args.output / 'replay.json').read_text())
        self.assertFalse(detail['reproduced'])
        self.assertIn('o.bin: values differ', detail['missing_difference_classes'])
        self.assertEqual(detail['reduction_attempts'], 0)

    def test_timeout_exit_races_are_reaped(self):
        for error in (PermissionError(), ProcessLookupError()):
            proc = Mock(pid=123, returncode=0)
            proc.poll.return_value = 0
            proc.communicate.side_effect = [subprocess.TimeoutExpired(['tool'], .01), (b'', b'')]
            with patch('common.subprocess.Popen', return_value=proc), patch('common.os.killpg', side_effect=error):
                result = invoke('tool', [], self.root, .01)
            self.assertTrue(result['timed_out'])
            self.assertFalse(result['budget_exhausted'])
            self.assertEqual(proc.communicate.call_count, 2)

    def test_budget_cuts_are_incomplete_evidence_not_timeout_failures(self):
        candidate = self.mutant('time.sleep(1)')
        status, report, output = self.workload(candidate, budget=.1, timeout=2)
        self.assertEqual(status, 1)
        self.assertFalse(report['complete'])
        self.assertFalse(report['failures'])
        self.assertTrue(report['incomplete_bundles'])
        self.assertEqual(report['coverage']['binary/plain']['candidate_timeouts'], 0)
        self.assertEqual(report['coverage']['binary/plain']['candidate_budget_cuts'], 1)
        self.assertFalse(report['results'][0]['differences'])
        args = Namespace(bundle=output / report['incomplete_bundles'][0], output=self.root / 'replay-cut')
        with redirect_stdout(io.StringIO()):
            self.assertEqual(replay(args), 2)
        self.assertFalse(json.loads((args.output / 'replay.json').read_text())['reproduced'])

    def test_genuine_timeout_is_still_a_failure_bundle(self):
        candidate = self.mutant('time.sleep(1)')
        status, report, _ = self.workload(candidate, budget=5, timeout=.05)
        self.assertEqual(status, 1)
        self.assertTrue(report['failures'])
        self.assertFalse(report['incomplete_bundles'])
        self.assertEqual(report['coverage']['binary/plain']['candidate_timeouts'], 1)
        self.assertEqual(report['coverage']['binary/plain']['candidate_budget_cuts'], 0)

    def test_startup_and_unexpected_errors_preserve_report(self):
        status, report, output = self.workload(self.root / 'missing-tools')
        self.assertEqual(status, 1)
        self.assertEqual(report['state'], 'error')
        self.assertTrue((output / 'error.txt').exists())
        with patch.object(runner, 'execute', side_effect=RuntimeError('injected harness error')):
            status, report, output = self.workload(self.baseline)
        self.assertEqual(status, 1)
        self.assertEqual(report['state'], 'error')
        self.assertEqual(report['omitted'], ['review-control/plain'])
        self.assertTrue((output / 'results.jsonl').exists())

    def test_sigterm_preserves_checkpoint_and_stops_child(self):
        candidate = self.mutant("Path('running').write_text(str(os.getpid()))\ntime.sleep(30)")
        output = self.root / 'signal-report'
        proc = subprocess.Popen([sys.executable, '-B', str(Path(runner.__file__)),
            '--baseline-dir', str(self.baseline), '--candidate-dir', str(candidate),
            '--binary-cases', '0', '--link-cases', '0', '--output', str(output)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 5
            while not (output / 'work/running').exists() and time.monotonic() < deadline:
                time.sleep(.01)
            self.assertTrue((output / 'work/running').exists())
            self.assertFalse(json.loads((output / 'report.json').read_text())['complete'])
            proc.terminate()
            stdout, stderr = proc.communicate(timeout=5)
            self.assertEqual(proc.returncode, 1, stdout + stderr)
            report = json.loads((output / 'report.json').read_text())
            self.assertEqual(report['state'], 'interrupted')
            self.assertTrue(report['omitted'])
        finally:
            if proc.poll() is None:
                proc.kill()
            proc.communicate()

    def test_sigterm_during_replay_and_reduction_stops_the_active_tool(self):
        mutation = "p=Path('o.bin'); p.write_bytes(p.read_bytes()+b'X')"
        status, report, original = self.workload(self.mutant(mutation))
        self.assertEqual(status, 1)
        bundle = original / report['failures'][0]
        for reducing in (False, True):
            with self.subTest(reducing=reducing):
                counter = self.root / f'invocations-{reducing}'
                threshold = 2 if reducing else 1
                candidate = self.mutant(mutation + '\n' +
                    f'p=Path({str(counter)!r}); n=int(p.read_text())+1 if p.exists() else 1\n'
                    f'p.write_text(str(n))\nif n >= {threshold}:\n'
                    " Path('running').write_text(str(os.getpid()))\n time.sleep(30)")
                output = self.root / f'signal-replay-{reducing}'
                command = [sys.executable, '-B', str(Path(runner.__file__).with_name('replay.py')),
                           str(bundle), '--candidate-dir', str(candidate), '--output', str(output),
                           '--timeout', '20'] + (['--reduce'] if reducing else [])
                proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                child_pid = None
                try:
                    ready = output / 'work/running'
                    self.wait_for(lambda: ready.exists() and ready.stat().st_size > 0)
                    child_pid = int(ready.read_text())
                    self.assertEqual(int(counter.read_text()), threshold)
                    proc.terminate()
                    stdout, stderr = proc.communicate(timeout=5)
                    self.assertEqual(proc.returncode, 130, stdout + stderr)
                    self.assertIn(b'Replay interrupted', stderr)
                    with self.assertRaises(ProcessLookupError):
                        os.kill(child_pid, 0)
                    child_pid = None
                finally:
                    if proc.poll() is None:
                        proc.kill()
                    if child_pid is not None:
                        try:
                            os.kill(child_pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                    proc.communicate()

    def test_content_counts_and_all_configuration_rejections(self):
        self.assertEqual(product_metrics('i.json', b'[]')['records'], {'records': 0})
        self.assertEqual(product_metrics('i.json', b'[{"site_addr":"0x8000"}]')['records'], {'records': 1})
        self.assertEqual(product_metrics('i.text', b'header\n')['measurement'], 'bytes only')
        self.assertEqual(product_metrics('s.json', b'{"top_data_labels": []}')['records'],
                         {'top_data_labels': 0})
        source = b'Unused .equ 7\n.org 0\nnop\n'
        status, report, _ = self.workload(self.baseline, {'plain': [],
            'nl': CONFIGS['nl-rom-listing'], 'warnings': CONFIGS['warnings']}, source)
        self.assertEqual(status, 1)  # no accepted workload for these two configurations
        reasons = report['configuration_rejection_reasons']
        self.assertIn('FCEUX ROM export restriction', reasons['binary/nl']['candidate'])
        self.assertIn('unused equate (W0201)', reasons['binary/warnings']['candidate'])
        self.assertEqual(len(report['coverage_errors']), 4)
        self.assertEqual(rejection_categories([{'diagnostics': 'p.asm:4: error: operand out of range\n'}]),
                         rejection_categories([{'diagnostics': 'other.asm:99: error: operand out of range\n'}]))

    def test_new_configuration_that_both_tools_reject_cannot_pass(self):
        status, report, _ = self.workload(self.baseline, {'bad': ['--no-such-option']})
        self.assertEqual(status, 1)
        self.assertFalse(report['failures'])
        self.assertTrue(report['complete'])
        self.assertEqual(len(report['coverage_errors']), 2)
        self.assertEqual(zero_success_errors({'test': Counter(baseline_validated=1, candidate_validated=1)}), [])

    def test_compare_match_and_mismatch_paths_are_exercised(self):
        status, report, _ = self.workload(self.baseline, {name: CONFIGS[name] for name in
            ('plain', 'compare-json', 'compare-match-json', 'compare-text', 'compare-match-text')})
        self.assertEqual(status, 0)
        self.assertEqual([r['candidate']['steps'][0]['returncode'] for r in report['results']], [0, 5, 0, 5, 0])

    def test_compare_status_uses_the_common_prefix_after_source_reduction(self):
        tools = {name: {'path': path} for name, path in self.actual.items()}
        for fmt in ('text', 'json'):
            case = {'kind': 'binary', 'config': 'compare', 'existing': False,
                    'args': ['--compare=ref.bin', '--compare-format=' + fmt]}
            for source, reference, expected in (
                    (b'nop\n', b'\xea\x60', 0),
                    (b'nop\nrts\n', b'\xea', 0),
                    (b'nop\n', b'', 0),
                    (b'', b'\xea', 0),
                    (b'nop\n', b'\x60\xea', 5)):
                with self.subTest(format=fmt, source=source, reference=reference):
                    inputs = {'p.asm': b'.org $8000\n' + source, 'ref.bin': reference}
                    outcome = execute(case, inputs, tools, self.root / 'prefix', 2,
                                      time.monotonic() + 5)
                    self.assertTrue(outcome['accepted'])
                    self.assertEqual(outcome['steps'][-1]['returncode'], expected)
                    self.assertEqual(outcome['errors'], [])
        candidate = self.mutant('status=5')
        tools['xasm']['path'] = str(candidate / 'xasm')
        outcome = execute(case, {'p.asm': b'.org $8000\nnop\n', 'ref.bin': b'\xea\x60'},
                          tools, self.root / 'prefix', 2, time.monotonic() + 5)
        self.assertIn('comparison status disagrees with emitted/reference bytes', outcome['errors'])

    def test_identical_tools_pass_the_complete_product_matrix(self):
        status, report, _ = self.workload(self.baseline, CONFIGS)
        self.assertEqual(status, 0, report['failures'])
        self.assertEqual(len(report['results']), len(CONFIGS))

    def test_cache_is_private_and_rejects_unsafe_entries(self):
        with patch('build_reference.Path.home', return_value=self.root), patch.dict(os.environ, {}, clear=True):
            self.assertTrue(default_cache().is_relative_to(self.root))
        cache = private_cache(self.root / 'cache')
        self.assertEqual(cache.stat().st_mode & 0o777, 0o700)
        cache.chmod(0o777)
        with self.assertRaises(ValueError):
            private_cache(cache)
        self.assertEqual(cache.stat().st_mode & 0o777, 0o777)
        cache.chmod(0o700)
        link = self.root / 'cache-link'
        link.symlink_to(cache)
        with self.assertRaises(ValueError):
            private_cache(link)
        with patch('build_reference.os.getuid', return_value=os.getuid() + 1), self.assertRaises(ValueError):
            require_owned(cache, directory=True)
        parent = self.root / 'unsafe-parent'
        parent.mkdir()
        parent.chmod(0o775)
        with self.assertRaisesRegex(ValueError, 'unsafe writable parent'):
            private_cache(parent / 'new' / 'cache')
        self.assertEqual(parent.stat().st_mode & 0o777, 0o775)
        self.assertFalse((parent / 'new').exists())

    def test_default_cache_creates_private_parents_independently_of_umask(self):
        for system in ('Linux', 'Darwin'):
            for mask in (0o002, 0o022, 0o077):
                with self.subTest(system=system, umask=oct(mask)):
                    home = (self.root / f'home-{system}-{mask:o}').resolve()
                    home.mkdir()
                    home.chmod(0o755)
                    with patch('build_reference.Path.home', return_value=home), \
                            patch('build_reference.platform.system', return_value=system), \
                            patch.dict(os.environ, {}, clear=True):
                        previous = os.umask(mask)
                        try:
                            cache = private_cache(default_cache())
                            self.assertEqual(private_cache(default_cache()), cache)
                        finally:
                            os.umask(previous)
                    self.assertEqual(home.stat().st_mode & 0o777, 0o755)
                    while cache != home:
                        self.assertEqual(cache.stat().st_mode & 0o777, 0o700)
                        cache = cache.parent

    def test_reference_build_and_cache_hit_ignore_umask(self):
        # Stub only Git/compiler subprocesses so ordinary make check needs no
        # pinned history or reference compilation. Exercise the real packaging,
        # permissions, receipt and cache-hit validation with writable build tools.
        def build(command, **kwargs):
            if command[0] == 'git' and command[3] == 'archive':
                with tarfile.open(fileobj=kwargs['stdout'], mode='w'):
                    pass
            elif command[0] == 'git' and command[3] == 'cat-file':
                pass
            elif command[0] == './configure':
                pass
            elif command == ['make', '-j2']:
                for name in ('xasm', 'xlnk'):
                    tool = Path(kwargs['cwd']) / name
                    tool.write_text(f'reference {name}\n')
                    tool.chmod(0o775)
            else:
                self.fail(f'unexpected reference build command: {command}')

        for mask in (0o002, 0o022, 0o077):
            with self.subTest(umask=oct(mask)), \
                    patch('build_reference.shutil.which', return_value='/test/cc'), \
                    patch('build_reference.subprocess.check_output', return_value='test compiler\n'), \
                    patch('build_reference.subprocess.run', side_effect=build) as commands, \
                    patch('build_reference.run_build_command', commands):
                previous = os.umask(mask)
                try:
                    cache = self.root / f'explicit-{mask:o}' / 'cache'
                    target = build_reference(cache, repo=self.root)
                    self.assertEqual(commands.call_count, 4)
                    modes = {cache.parent: 0o700, cache: 0o700, target: 0o700,
                             target / 'xasm': 0o755, target / 'xlnk': 0o755,
                             target / 'build.json': 0o644,
                             cache / (target.name + '.log'): 0o644,
                             cache / (target.name + '.lock'): 0o600}
                    for path, mode in modes.items():
                        self.assertEqual(path.stat().st_mode & 0o777, mode, str(path))
                    commands.reset_mock()
                    self.assertEqual(build_reference(cache, repo=self.root), target)
                    commands.assert_not_called()
                    # A cache hit must still reject entries made writable after
                    # publication, without silently repairing them or rebuilding.
                    for path in (target, target / 'xasm', target / 'xlnk', target / 'build.json',
                                 cache / (target.name + '.lock')):
                        path.chmod(modes[path] | 0o020)
                        with self.assertRaisesRegex(ValueError, 'not writable by others'):
                            build_reference(cache, repo=self.root)
                        self.assertEqual(path.stat().st_mode & 0o777, modes[path] | 0o020)
                        path.chmod(modes[path])
                    commands.assert_not_called()
                finally:
                    os.umask(previous)

    def test_build_timeout_and_interruption_stop_descendants_and_reap_driver(self):
        for interrupted in (False, True):
            with self.subTest(interrupted=interrupted):
                ready = self.root / f'child-{interrupted}'
                reader, writer = os.pipe()
                writer_open = True
                launched = []
                popen = subprocess.Popen
                child_code = ('import os,sys,time\nfrom pathlib import Path\n'
                              'Path(sys.argv[1]).write_text(str(os.getpid()))\ntime.sleep(10)\n')
                driver_code = ('import subprocess,sys,time\n'
                               f'subprocess.Popen([sys.executable,"-BI","-c",{child_code!r},sys.argv[1]],'
                               'pass_fds=(int(sys.argv[2]),))\ntime.sleep(10)\n')

                def launch(*args, **kwargs):
                    nonlocal writer_open
                    proc = popen(*args, **kwargs)
                    launched.append(proc)
                    os.close(writer)
                    writer_open = False
                    wait = proc.wait

                    def synchronized_wait(timeout=None):
                        if timeout is not None:
                            self.wait_for(lambda: ready.exists() and ready.stat().st_size > 0)
                            if interrupted:
                                raise KeyboardInterrupt('test interruption')
                        return wait(timeout=timeout)

                    proc.wait = synchronized_wait
                    return proc

                try:
                    exception = KeyboardInterrupt if interrupted else subprocess.TimeoutExpired
                    with patch('build_reference.subprocess.Popen', side_effect=launch), \
                            self.assertRaises(exception):
                        run_build_command([sys.executable, '-BI', '-c', driver_code, str(ready), str(writer)],
                                          timeout=0.05, pass_fds=(writer,),
                                          stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
                    self.assertEqual(len(launched), 1)
                    self.assertLess(launched[0].returncode, 0)
                    with self.assertRaises(ChildProcessError):
                        os.waitpid(launched[0].pid, os.WNOHANG)
                    # Both driver and descendant inherited this pipe. EOF proves
                    # they closed it, even if an orphan briefly remains a zombie.
                    self.assertTrue(select.select([reader], [], [], 2)[0], 'build descendant survived')
                    self.assertEqual(os.read(reader, 1), b'')
                finally:
                    if writer_open:
                        os.close(writer)
                    os.close(reader)
                    for proc in launched:
                        if proc.poll() is None:
                            proc.kill()
                        proc.wait()
                    if ready.exists():
                        try:
                            os.kill(int(ready.read_text()), signal.SIGKILL)
                        except ProcessLookupError:
                            pass

    def test_concurrent_reference_builds_share_one_publication_and_log(self):
        script = self.root / 'concurrent.py'
        script.write_text(textwrap.dedent('''\
            from contextlib import contextmanager
            import os
            from pathlib import Path
            import sys
            import tarfile
            import time
            from unittest.mock import patch
            harness, cache, control = map(Path, sys.argv[1:])
            sys.path.insert(0, str(harness))
            import build_reference as builder
            original_lock = builder.reference_lock
            @contextmanager
            def lock(path):
                (control / f'lock-{os.getpid()}').touch()
                with original_lock(path):
                    yield
            def git(command, **kwargs):
                if command[0] != 'git':
                    raise AssertionError(command)
                if command[3] == 'archive':
                    with tarfile.open(fileobj=kwargs['stdout'], mode='w'):
                        pass
                elif command[3] != 'cat-file':
                    raise AssertionError(command)
            def build(command, **kwargs):
                if command[0] == './configure':
                    (control / f'build-{os.getpid()}').touch()
                    kwargs['stdout'].write(f'owner={os.getpid()}\\n'.encode())
                    kwargs['stdout'].flush()
                    deadline = time.monotonic() + 10
                    while not (control / 'release').exists():
                        if time.monotonic() > deadline:
                            raise RuntimeError('test build was not released')
                        time.sleep(0.01)
                elif command == ['make', '-j2']:
                    for name in ('xasm', 'xlnk'):
                        tool = Path(kwargs['cwd']) / name
                        tool.write_text(f'reference {name}\\n')
                        tool.chmod(0o775)
                else:
                    raise AssertionError(command)
            with patch.object(builder, 'reference_lock', lock), patch.object(builder, 'run_build_command', build), \\
                    patch.object(builder.shutil, 'which', return_value='/test/cc'), \\
                    patch.object(builder.subprocess, 'check_output', return_value='test compiler\\n'), \\
                    patch.object(builder.subprocess, 'run', git):
                print(builder.build_reference(cache))
            '''))
        control = self.root / 'concurrent-control'
        control.mkdir()
        cache = self.root / 'concurrent-cache'
        command = [sys.executable, '-BI', str(script), str(Path(__file__).with_name('differential')),
                   str(cache), str(control)]
        processes = []
        try:
            processes.append(subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE))
            self.wait_for(lambda: len(list(control.glob('build-*'))) == 1)
            processes.append(subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE))
            self.wait_for(lambda: len(list(control.glob('lock-*'))) == 2)
            time.sleep(0.05)
            self.assertEqual(len(list(control.glob('build-*'))), 1)
            self.assertTrue(all(proc.poll() is None for proc in processes))
            (control / 'release').touch()
            targets = []
            for proc in processes:
                stdout, stderr = proc.communicate(timeout=10)
                self.assertEqual(proc.returncode, 0, stderr.decode())
                targets.append(Path(stdout.decode().strip()))
            self.assertEqual(targets[0], targets[1])
            builders = list(control.glob('build-*'))
            self.assertEqual(len(builders), 1)
            log = (cache / (targets[0].name + '.log')).read_text()
            self.assertEqual(log.count('owner='), 1)
            self.assertIn('owner=' + builders[0].name.split('-')[1], log)
            self.assertIn('make -j2', log)
            self.assertTrue((targets[0] / 'build.json').is_file())
            self.assertEqual((targets[0] / 'xasm').read_text(), 'reference xasm\n')
        finally:
            (control / 'release').touch()
            for proc in processes:
                if proc.poll() is None:
                    proc.kill()
                proc.communicate()


if __name__ == '__main__':
    unittest.main()
