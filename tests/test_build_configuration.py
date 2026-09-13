#!/usr/bin/env python3
"""Exercise generated configure and Makefile rules in an isolated source copy."""
import os
import re
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BuildConfiguration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temporary = tempfile.TemporaryDirectory(prefix='xasm-build-tests-')
        cls.addClassCleanup(temporary.cleanup)
        cls.root = Path(temporary.name)
        cls.source = cls.root / 'source'
        cls.source.mkdir()
        build_files = {'configure', 'configure.ac', 'Makefile.am', 'Makefile.in',
                       'aclocal.m4', 'config.h.in', 'config.guess', 'config.sub',
                       'install-sh', 'missing', 'depcomp', 'compile'}
        for path in ROOT.iterdir():
            if path.name in build_files or (path.suffix in ('.c', '.h') and path.name != 'config.h'):
                shutil.copy2(path, cls.source / path.name)

    def setUp(self):
        self.build = self.root / self._testMethodName
        self.build.mkdir()

    def configure(self, **environment):
        return subprocess.run(['sh', str(self.source / 'configure'), '--disable-maintainer-mode'],
                              cwd=self.build, capture_output=True, timeout=60,
                              env={**os.environ, 'CFLAGS': '-O0', **environment})

    def test_global_link_additions_reach_both_programs(self):
        configured = self.configure()
        self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
        version = re.search(r'^#define XORCYST_VERSION "([^"]+)"',
                            (self.source / 'version.h').read_text(), re.M)[1]
        self.assertIn(f'#define PACKAGE_VERSION "{version}"',
                      (self.build / 'config.h').read_text())
        probe = self.build / 'probe.c'
        probe.write_text('int xasm_ldadd_probe(void) { return 42; }\n')
        object_file = self.build / 'probe.o'
        subprocess.run([*shlex.split(os.environ.get('CC', 'cc')), '-c', str(probe), '-o', str(object_file)],
                       check=True, capture_output=True)
        built = subprocess.run([os.environ.get('MAKE', 'make'), '-j2', f'LDADD={object_file}', 'xasm', 'xlnk'],
                               cwd=self.build, capture_output=True, timeout=120)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        for program in ('xasm', 'xlnk'):
            with self.subTest(program=program):
                symbols = subprocess.check_output(['nm', str(self.build / program)])
                self.assertTrue(b'xasm_ldadd_probe' in symbols, f'{program} did not link the LDADD object')
                run = subprocess.run([str(self.build / program), '--help'], capture_output=True)
                self.assertEqual(run.returncode, 0, run.stderr)
                reported = subprocess.check_output([str(self.build / program), '--version'])
                self.assertEqual(reported, f'{program} {version}\n'.encode())

    @unittest.skipUnless(sys.platform == 'darwin', 'Darwin SDK requirement')
    def test_missing_corefoundation_headers_fail_at_configure(self):
        headers = self.build / 'sdk'
        (headers / 'CoreFoundation').mkdir(parents=True)
        (headers / 'CoreFoundation/CoreFoundation.h').write_text('#error CoreFoundation headers unavailable\n')
        result = self.configure(CPPFLAGS=f'-I{shlex.quote(str(headers))}')
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(b'CoreFoundation headers and framework are required', result.stderr)

    @unittest.skipUnless(sys.platform == 'darwin', 'Darwin SDK requirement')
    def test_missing_corefoundation_framework_fails_at_configure(self):
        compiler = self.build / 'cc-without-corefoundation'
        compiler.write_text('#!/bin/sh\nfor arg do\n'
                            '  if test "$arg" = CoreFoundation; then\n'
                            '    echo "test: CoreFoundation framework unavailable" >&2\n'
                            '    exit 1\n  fi\ndone\n'
                            + 'exec ' + shlex.join(shlex.split(os.environ.get('CC', 'cc'))) + ' "$@"\n')
        compiler.chmod(0o755)
        result = self.configure(CC=str(compiler))
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(b'CoreFoundation headers and framework are required', result.stderr)


if __name__ == '__main__':
    unittest.main()
