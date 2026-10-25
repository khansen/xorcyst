#!/usr/bin/env python3
"""Run the existing output contracts through the owned v2 backend.

V2-only policy assertions are explicit in the reused suites: structured
analysis refuses objects; explicit-address .label and a +128 branch reject.
All successful pure-binary record, symbol, exporter and access checks remain.
"""
import importlib
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

from test_v2_frontend import build

REPO = Path(__file__).resolve().parents[1]
TOOLS = tempfile.TemporaryDirectory(prefix='xorcyst-i5-tools-')


def load_tests(loader, tests, pattern):
    assembler = Path(TOOLS.name) / 'xasm-v2'
    linker = Path(TOOLS.name) / 'xlnk-v2'
    build(assembler)
    sources = ('v2/linker.c', 'v2/link.c', 'v2/object.c', 'v2/heap.c', 'v2/value.c',
               'v2/layout.c', 'opcode.c', 'script.c', 'output_file.c', 'my_getopt.c')
    command = [*shlex.split(os.environ.get('CC', 'cc')), '-I', str(REPO), '-Wall',
               *shlex.split(os.environ.get('TEST_V2_CFLAGS', '-O2 -g -Werror')),
               *[str(REPO / p) for p in sources], '-o', str(linker)]
    run = subprocess.run(command, capture_output=True, text=True)
    if run.returncode: raise AssertionError(run.stderr)
    argv = sys.argv
    try:
        sys.argv = [argv[0]]
        for name in ('test_instruction_records', 'test_analysis_symbols',
                     'test_access_classification', 'test_fceux_nl', 'test_output_failures'):
            module = importlib.import_module(name)
            module.XASM, module.XLNK, module.V2_ANALYSIS = assembler, linker, True
            tests.addTests(loader.loadTestsFromModule(module))
    finally:
        sys.argv = argv
    return tests


if __name__ == '__main__':
    try:
        unittest.main()
    finally:
        TOOLS.cleanup()
