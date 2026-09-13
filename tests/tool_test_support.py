"""Build real command-line tools with optional test-only translation units."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]
SANITIZER_REPORT = rb'(?:AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer):|runtime error:'


def build_tool(name, destination, flag_variable, replacements=None):
    sources = re.search(r'^' + name + r'_SOURCES = (.*?)(?=\n\n)',
                        (REPO / 'Makefile.am').read_text(), re.M | re.S)[1]
    replacements = replacements or {}
    sources = [REPO / replacements.get(word, word)
               for word in sources.replace('\\\n', ' ').split()
               if word.endswith('.c') and replacements.get(word, word) is not None]
    command = [*shlex.split(os.environ.get('CC', 'cc')), '-I', str(REPO), '-Wall',
               *shlex.split(os.environ.get(flag_variable, '-O0 -g')),
               *map(str, sources), '-o', str(destination)]
    if name == 'xasm' and sys.platform == 'darwin':
        command += ['-framework', 'CoreFoundation']
    result = subprocess.run(command, capture_output=True, timeout=120)
    if result.returncode:
        raise AssertionError(result.stderr.decode())


def object_file(code=b'\xf3', data=b'\xf3', expressions=()):
    """Encode a minimal object using the documented big-endian object format."""
    return (bytes.fromhex('face14') + b'\0\0\0\0\0'
            + len(data).to_bytes(3, 'big') + data
            + len(code).to_bytes(3, 'big') + code
            + len(expressions).to_bytes(2, 'big') + b''.join(expressions))
