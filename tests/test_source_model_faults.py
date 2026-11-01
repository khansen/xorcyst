#!/usr/bin/env python3
"""Real producer fault controls, kept outside the installed producer."""
import json
import os
import re
from pathlib import Path
import subprocess
import tempfile
import unittest
from tool_test_support import build_tool, SANITIZER_REPORT
from source_model_support import validate, ModelError


class FaultTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix="source-model-fault-build-")
        cls.addClassCleanup(cls.directory.cleanup)
        cls.tool = Path(cls.directory.name) / "xasm-model-faults"
        build_tool("xasm", cls.tool, "TEST_SOURCE_MODEL_CFLAGS",
                   {"source_model.c": "tests/test_source_model_faults.c"})
        cls.usage = Path(cls.directory.name) / "xasm-usage-faults"
        build_tool("xasm", cls.usage, "TEST_SOURCE_MODEL_CFLAGS",
                   {"source_model.c": "tests/test_source_model_faults.c",
                    "ir/frontend.c": "tests/test_source_model_frontend_faults.c"})
        cls.io = Path(cls.directory.name) / "xasm-io-faults"
        build_tool("xasm", cls.io, "TEST_SOURCE_MODEL_CFLAGS",
                   {"xasm.c": "tests/test_io_faults.c", "output_file.c": None, "fceux_nl.c": None})

    def run_case(self, source, environment, tool=None):
        with tempfile.TemporaryDirectory(prefix="source-model-fault-") as directory:
            root = Path(directory); (root / "input.asm").write_bytes(source)
            (root / "model").write_bytes(b"old model")
            (root / "out").write_bytes(b"old binary")
            result = subprocess.run([str(tool or self.tool), "--pure-binary", "--source-model=model",
                                     "--source-model-diagnostics=diag", "input.asm", "-o", "out"], cwd=root,
                                    env={**os.environ, **environment}, capture_output=True, timeout=20)
            self.assertNotRegex(result.stderr, SANITIZER_REPORT)
            self.assertGreaterEqual(result.returncode, 0, result.stderr)
            after = (root / "model").read_bytes(), (root / "out").read_bytes()
            diagnostic = json.loads((root / "diag").read_bytes()) if (root / "diag").exists() else None
            model = json.loads(after[0]) if after[0] != b"old model" else None
            return result, after, diagnostic, model

    def test_policy_guard_mutants_have_teeth(self):
        for source, mutation, code in (
            (b".DEFINE Value 7\nLDA #Value\n", "capability", "SOURCE_MODEL_UNSUPPORTED_CONSTRUCT"),
            (b"V .EQU +\nNOP\n+ NOP\nJMP V\n+ NOP\n", "capability", "SOURCE_MODEL_UNSUPPORTED_CONSTRUCT"),
            (b"Value .EQU 7\nValue .EQU 7\n", "duplicate", "SOURCE_MODEL_DUPLICATE_DECLARATION"),
        ):
            result, _, diagnostic, _ = self.run_case(source, {})
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn(code, {d["code"] for d in diagnostic["diagnostics"]})
            result, _, diagnostic, _ = self.run_case(source, {"XASM_TEST_MODEL_MUTATION": mutation})
            codes = {d["code"] for d in diagnostic["diagnostics"]} if diagnostic else set()
            self.assertNotIn(code, codes, "guard mutation did not disable the exercised guard")
        result, _, _, model = self.run_case(b"Cell .EQU 7\nValue .EQU Cell+1\n", {})
        self.assertEqual(result.returncode, 0, result.stderr); validate(model)
        result, _, diagnostic, _ = self.run_case(b"Cell .EQU 7\nValue .EQU Cell+1\n",
                                                {"XASM_TEST_MODEL_MUTATION": "unused"})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("SOURCE_MODEL_VALUE_UNAVAILABLE", {d["code"] for d in diagnostic["diagnostics"]})

    def test_collector_and_access_mutants(self):
        source = b"Cell .EQU $10\nSTA Cell\n"
        result, _, _, model = self.run_case(source, {})
        self.assertEqual(result.returncode, 0, result.stderr)
        validate(model)
        for mutation in ("collector", "access"):
            result, _, _, model = self.run_case(source, {"XASM_TEST_MODEL_MUTATION": mutation})
            if result.returncode == 0:
                with self.assertRaises(ModelError):
                    validate(model)
            else:
                self.assertIn(result.returncode, (1, 3), result.stderr)

    def test_changed_inputs_never_publish(self):
        for mutation in ("input-bytes", "input-delete", "input-symlink", "input-missing"):
            source = b'.INCLUDE "late.inc"\nNOP\n' if mutation == "input-missing" else b"NOP\n"
            result, after, _, _ = self.run_case(source, {"XASM_TEST_MODEL_MUTATION": mutation})
            self.assertEqual(result.returncode, 3, (mutation, result.stderr))
            self.assertIn(b"SOURCE_MODEL_INPUT_CHANGED", result.stderr)
            self.assertEqual(after, (b"old model", b"old binary"))

    def test_wrong_native_binding(self):
        source = b"One .EQU 8\nOther .EQU 8\nLDA #One\n"
        result, _, _, model = self.run_case(source, {"XASM_TEST_MODEL_MUTATION": "binding"})
        # A producer cross-check may reject before publication; otherwise the
        # independent reader must reject despite equal native values and bytes.
        if result.returncode == 0:
            with self.assertRaisesRegex(ModelError, "MODEL_BINDING_TARGET"):
                validate(model)
        else:
            self.assertEqual(result.returncode, 1, result.stderr)

    def test_internal_layout_failure(self):
        result, after, diagnostic, _ = self.run_case(b"NOP\n", {"XASM_TEST_MODEL_MUTATION": "layout"})
        self.assertEqual(result.returncode, 3, result.stderr)
        self.assertIn(b"SOURCE_MODEL_INTERNAL_ERROR", result.stderr)
        self.assertIn("SOURCE_MODEL_INTERNAL_ERROR", {d["code"] for d in diagnostic["diagnostics"]})
        self.assertEqual(after, (b"old model", b"old binary"))

    def test_analysis_usage_fault(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "in.asm").write_bytes(b"Cell .EQU 7\nAlias .EQU Cell+1\nLDA #Alias\n")
            for mutation in (None, "usage"):
                env = dict(os.environ)
                if mutation: env["XASM_TEST_MODEL_MUTATION"] = mutation
                r = subprocess.run([str(self.usage), "--pure-binary", "in.asm", "-o", "out", "--source-model=model"], cwd=root, env=env, capture_output=True)
                self.assertEqual(r.returncode, 0, r.stderr)
                deltas = [int(n) for n in re.findall(rb"SHADOW_USE_DELTA (-?[0-9]+)", r.stderr)]
                self.assertTrue(deltas, "native shadow path was not observed")
                def unchanged():
                    self.assertTrue(all(n == 0 for n in deltas), "ANALYSIS_CHANGED_USAGE")
                if mutation:
                    self.assertIn(b"INJECT_ANALYSIS_USAGE", r.stderr)
                    with self.assertRaisesRegex(AssertionError, "ANALYSIS_CHANGED_USAGE"): unchanged()
                else: unchanged()

    def test_allocation_failures_preserve_previous_artifacts(self):
        source = b".ORG $8000\nOne .EQU 8\nEntry: LDA #One\n.DW Entry\n"
        reached_end = False
        for at in range(300):
            result, after, _, model = self.run_case(source, {"XASM_TEST_MODEL_ALLOC_AT": str(at)})
            if b"INJECT_MODEL_ALLOC" not in result.stderr:
                self.assertEqual(result.returncode, 0, result.stderr)
                validate(model); reached_end = True; break
            self.assertEqual(result.returncode, 3, (at, result.stderr))
            self.assertIn(b"SOURCE_MODEL_RESOURCE_LIMIT", result.stderr)
            self.assertEqual(after, (b"old model", b"old binary"), (at, result.stderr))
        self.assertTrue(reached_end, "allocation walk did not reach the successful control")

    def test_model_publication_faults(self):
        for operation in ("output_malloc", "output_mkstemp", "output_fdopen", "output_ferror",
                          "output_fclose", "output_fflush", "output_rename"):
            with self.subTest(operation=operation):
                result, after, _, _ = self.run_case(b"NOP\n",
                    {"XASM_TEST_IO_FAILURE": operation, "XASM_TEST_OUTPUT_PATH": "model"}, self.io)
                self.assertIn(b"INJECT_IO", result.stderr)
                self.assertIn(b"SOURCE_MODEL_IO_ERROR", result.stderr)
                self.assertEqual(result.returncode, 3, result.stderr)
                self.assertEqual(after, (b"old model", b"old binary"))


if __name__ == "__main__":
    unittest.main()
