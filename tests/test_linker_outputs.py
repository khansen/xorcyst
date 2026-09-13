#!/usr/bin/env python3
"""Exercise linker publication through real write limits and injected I/O errors."""
import os
from pathlib import Path
import resource
import signal
import stat
import subprocess
import tempfile
import unittest

from tool_test_support import build_tool, object_file, SANITIZER_REPORT


class LinkerOutputs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory(prefix='xlnk-output-build-')
        cls.addClassCleanup(build.cleanup)
        cls.executable = Path(build.name) / 'xlnk'
        # The wrapper includes the real writer and CLI in one translation unit.
        # Compile the remaining linker sources normally.
        build_tool('xlnk', cls.executable, 'TEST_LINKER_CFLAGS',
                   {'xlnk.c': 'tests/test_xlnk_io_faults.c', 'output_file.c': None})

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='xlnk-output-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / 'payload').write_bytes(bytes(range(256)) * 32)

    def run_linker(self, script, default=True, fault=None, target=None, limit=False):
        (self.root / 'link.script').write_text(script)
        environment = {k: v for k, v in os.environ.items() if not k.startswith('XLNK_TEST_')}
        if fault:
            environment['XLNK_TEST_FAULT'] = fault
        if target:
            environment['XLNK_TEST_OUTPUT'] = target

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

    def test_file_size_limit_preserves_default_and_script_outputs(self):
        for default in (False, True):
            for content in ('copy{file=payload}', 'pad{size=8192}'):
                with self.subTest(default=default, content=content):
                    self.seed()
                    script = ('' if default else 'output{file=first}\n') + content + '\noutput{file=second}\n'
                    result = self.run_linker(script, default=default, limit=True)
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    self.assertEqual((self.root / 'first').read_bytes(), b'previous first')
                    self.assertEqual((self.root / 'second').read_bytes(), b'previous second')

    def test_staging_close_and_publish_failures_stop_output_switches(self):
        for operation in ('create', 'flush', 'close', 'rename'):
            for target in ('first', 'second'):
                with self.subTest(operation=operation, target=target):
                    self.seed()
                    result = self.run_linker('pad{size=3}\noutput{file=second}\npad{size=5}\n',
                                             fault=operation, target=target)
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    self.assertIn(('INJECT ' + operation).encode(), result.stderr)
                    self.assertEqual((self.root / 'first').read_bytes(),
                                     b'previous first' if target == 'first' else b'\0' * 3)
                    self.assertEqual((self.root / 'second').read_bytes(), b'previous second')
                    for name in ('first', 'second'):
                        self.assertEqual(stat.S_IMODE((self.root / name).stat().st_mode), 0o664)

    def test_copy_read_and_close_errors_preserve_destination(self):
        for operation in ('read', 'read_close'):
            with self.subTest(operation=operation):
                self.seed()
                result = self.run_linker('copy{file=payload}\n', fault=operation)
                self.assertEqual(result.returncode, 1, result.stderr.decode())
                self.assertIn(('INJECT ' + operation).encode(), result.stderr)
                self.assertEqual((self.root / 'first').read_bytes(), b'previous first')

    def test_successful_switches_finish_each_file_and_keep_copy_order(self):
        self.seed()
        result = self.run_linker('pad{size=3}\noutput{file=second}\ncopy{file=first}\npad{size=2}\n')
        # Preflight sees the old first file; output copying must see its newly published bytes.
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual((self.root / 'first').read_bytes(), b'\0' * 3)
        self.assertEqual((self.root / 'second').read_bytes(), b'\0' * 5)

    def test_linked_bytes_and_final_bank_padding_are_checked(self):
        (self.root / 'unit.o').write_bytes(object_file(code=bytes.fromhex('f401ea60f3')))
        result = self.run_linker('bank{size=8,origin=$8000}\nlink{file=unit.o}\n')
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual((self.root / 'first').read_bytes(), bytes.fromhex('ea60') + b'\0' * 6)
        self.assertEqual(stat.S_IMODE((self.root / 'first').stat().st_mode), 0o640)
        self.seed()
        result = self.run_linker('bank{size=8192,origin=$8000}\nlink{file=unit.o}\n', limit=True)
        self.assertEqual(result.returncode, 1, result.stderr.decode())
        self.assertEqual((self.root / 'first').read_bytes(), b'previous first')


if __name__ == '__main__':
    unittest.main()
