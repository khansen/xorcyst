#!/usr/bin/env python3
"""Publication, copy and file-size failures through the installed linker path.

Malformed objects and allocation failures are exercised by test_ir_objects;
this suite retains CLI/filesystem checks independently of the wire encoder.
"""
import os
from pathlib import Path
import resource
import signal
import stat
import subprocess
import tempfile
import unittest

from tool_test_support import build_tool, SANITIZER_REPORT


class LinkerOutputs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory(prefix='xlnk-output-build-')
        cls.addClassCleanup(build.cleanup)
        cls.executable = Path(build.name) / 'xlnk'
        cls.assembler = Path(build.name) / 'xasm'
        build_tool('xlnk', cls.executable, 'TEST_LINKER_CFLAGS',
                   {'output_file.c': 'tests/test_ir_link_io.c'})
        build_tool('xasm', cls.assembler, 'TEST_LINKER_CFLAGS')

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='xlnk-output-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / 'payload').write_bytes(bytes(range(256)) * 32)

    def run_linker(self, script, default=True, fault=None, target=None, limit=False):
        (self.root / 'link.script').write_text(script)
        environment = {k: v for k, v in os.environ.items() if not k.startswith('XLNK_TEST_')}
        if fault: environment['XLNK_TEST_FAULT'] = fault
        if target: environment['XLNK_TEST_OUTPUT'] = target
        def set_limit():
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE, (1024, 1024))
        result = subprocess.run([str(self.executable), 'link.script', *(['-o', 'first'] if default else [])],
                                cwd=self.root, env=environment, capture_output=True, timeout=15,
                                preexec_fn=set_limit if limit else None, umask=0o027)
        self.assertGreaterEqual(result.returncode, 0, result.stderr.decode())
        self.assertNotRegex(result.stderr, SANITIZER_REPORT)
        self.assertEqual(list(self.root.glob('.xasm-*')), [])
        return result

    def seed(self):
        for name in ('first', 'second'):
            (self.root / name).write_bytes(b'previous ' + name.encode())
            (self.root / name).chmod(0o664)

    def assemble(self, source):
        (self.root / 'unit.asm').write_text(source)
        r = subprocess.run([str(self.assembler), 'unit.asm', '-o', 'unit.o'],
                           cwd=self.root, capture_output=True)
        self.assertEqual(r.returncode, 0, r.stderr.decode())

    def test_file_size_limit_preserves_all_outputs(self):
        for default in (False, True):
            for content in ('copy{file=payload}', 'pad{size=8192}'):
                with self.subTest(default=default, content=content):
                    self.seed()
                    script = ('' if default else 'output{file=first}\n') + content + '\noutput{file=second}\n'
                    result = self.run_linker(script, default=default, limit=True)
                    self.assertEqual(result.returncode, 3, result.stderr.decode())
                    for name in ('first', 'second'):
                        self.assertEqual((self.root / name).read_bytes(), b'previous ' + name.encode())

    def test_staging_failures_preserve_all_destinations(self):
        for operation in ('create', 'flush', 'permissions', 'close', 'rename'):
            for target in ('first', 'second'):
                with self.subTest(operation=operation, target=target):
                    self.seed()
                    r = self.run_linker('pad{size=3}\noutput{file=second}\npad{size=5}\n',
                                        fault=operation, target=target)
                    self.assertEqual(r.returncode, 3, r.stderr.decode())
                    self.assertIn(('INJECT ' + operation).encode(), r.stderr)
                    # The linker stages everything, then publishes in order. A
                    # failed rename can leave earlier destinations published.
                    for name in ('first', 'second'):
                        expected = b'\0' * 3 if operation == 'rename' and target == 'second' and name == 'first' else b'previous ' + name.encode()
                        self.assertEqual((self.root / name).read_bytes(), expected)
                        self.assertEqual(stat.S_IMODE((self.root / name).stat().st_mode), 0o664)

    def test_bad_copy_inputs_keep_outputs_and_report_script_location(self):
        (self.root / 'directory').mkdir()
        for name in ('missing', 'directory'):
            with self.subTest(name=name):
                self.seed()
                r = self.run_linker(f'pad{{size=3}}\noutput{{file=second}}\ncopy{{file={name}}}\n')
                self.assertEqual(r.returncode, 1, r.stderr.decode())
                self.assertIn(b'link.script:3:', r.stderr)
                self.assertNotIn(b'no error', r.stderr)
                for dest in ('first','second'):
                    self.assertEqual((self.root/dest).read_bytes(), b'previous '+dest.encode())

    def test_copy_length_and_relocated_units_keep_offsets(self):
        self.assemble('Here: .word Here\n')
        payload = (self.root / 'payload').read_bytes()
        for switched in (False, True):
            with self.subTest(switched=switched):
                script = 'pad{size=3}\noutput{file=second}\n' if switched else ''
                r = self.run_linker(script + 'copy{file=payload}\nlink{file=unit.o}\n')
                self.assertEqual(r.returncode, 0, r.stderr.decode())
                address = len(payload) + (3 if switched else 0)
                self.assertEqual((self.root / ('second' if switched else 'first')).read_bytes(),
                                 payload + address.to_bytes(2, 'little'))

    def test_copy_cannot_alias_an_output(self):
        self.seed()
        r = self.run_linker('pad{size=3}\noutput{file=second}\ncopy{file=first}\n')
        self.assertEqual(r.returncode, 1, r.stderr.decode())
        self.assertIn(b'aliases', r.stderr)
        for name in ('first','second'):
            self.assertEqual((self.root/name).read_bytes(), b'previous '+name.encode())

    def test_empty_and_repeated_copies_keep_their_sizes(self):
        for payload in (b'', b'X', bytes(range(256)) * 32):
            with self.subTest(size=len(payload)):
                (self.root / 'payload').write_bytes(payload)
                r = self.run_linker('copy{file=payload}\npad{size=1}\ncopy{file=payload}\n')
                self.assertEqual(r.returncode, 0, r.stderr.decode())
                self.assertEqual((self.root / 'first').read_bytes(), payload + b'\0' + payload)

    def test_linked_bytes_and_final_bank_padding_are_checked(self):
        self.assemble('nop\nrts\n')
        r = self.run_linker('bank{size=8,origin=$8000}\nlink{file=unit.o}\n')
        self.assertEqual(r.returncode, 0, r.stderr.decode())
        self.assertEqual((self.root / 'first').read_bytes(), bytes.fromhex('ea60') + b'\0' * 6)
        self.assertEqual(stat.S_IMODE((self.root / 'first').stat().st_mode), 0o640)
        self.seed()
        r = self.run_linker('bank{size=8192,origin=$8000}\nlink{file=unit.o}\n', limit=True)
        self.assertEqual(r.returncode, 3, r.stderr.decode())
        self.assertEqual((self.root / 'first').read_bytes(), b'previous first')


if __name__ == '__main__':
    unittest.main()
