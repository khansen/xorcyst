#!/usr/bin/env python3
"""Exercise both installed tools without relying on build-tree executables."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1]
BINDIR = Path(sys.argv.pop(1)).resolve()
DOCDIR = Path(sys.argv.pop(1)).resolve()
VERSION = re.search(r'^#define XORCYST_VERSION "([^"]+)"',
                    (SOURCE / 'version.h').read_text(), re.M)[1]


class Installation(unittest.TestCase):
    def test_installed_versions(self):
        for name in ('xasm', 'xlnk'):
            with self.subTest(tool=name):
                reported = subprocess.check_output([str(BINDIR / name), '--version'])
                self.assertEqual(reported, f'{name} {VERSION}\n'.encode())

    def test_installed_documentation(self):
        documents = ['README', 'NEWS', 'xorcyst.texinfo',
                     'XASM_DATA_DIRECTIVE_REFERENCES_SPEC.md', 'XASM_DEPENDENCY_MANIFEST_SPEC.md',
                     'XASM_FCEUX_NL_EXPORT_SPEC.md', 'XASM_INDEX_BOUND_ANALYSIS_SPEC.md',
                     'XASM_INSTRUCTION_RECORDS_SPEC.md']
        for name in documents:
            with self.subTest(document=name):
                self.assertEqual((DOCDIR / name).read_bytes(), (SOURCE / name).read_bytes())

    def test_installed_assembler_and_linker(self):
        with tempfile.TemporaryDirectory(prefix='xorcyst-installed-') as temporary:
            root = Path(temporary)
            source = 'Entry:\nLDA #$42\nRTS\n'
            (root / 'input.asm').write_text(source)
            def run(name, *arguments):
                result = subprocess.run([str(BINDIR / name), *arguments], cwd=root,
                                        capture_output=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            run('xasm', 'input.asm', '-o', 'input.o')
            (root / 'link.script').write_text('link{file=input.o,origin=$8000}\n')
            run('xlnk', 'link.script', '-o', 'linked.bin')
            expected = bytes.fromhex('a94260')
            self.assertEqual((root / 'linked.bin').read_bytes(), expected)
            (root / 'input.asm').write_text('.ORG $8000\n' + source)
            run('xasm', '--pure-binary', 'input.asm', '-o', 'raw.bin',
                '--fceux-nl-rom-prefix=game.nes.')
            self.assertEqual((root / 'raw.bin').read_bytes(), expected)
            self.assertEqual((root / 'game.nes.0.nl').read_text(), '$8000#Entry#\n')


if __name__ == '__main__':
    unittest.main()
