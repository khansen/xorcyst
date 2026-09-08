#!/usr/bin/env python3
"""Dependency manifests and deterministic snapshot-mutation contracts."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
XASM = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT / "xasm"


class Dependencies(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix="dependency-driver-")
        cls.addClassCleanup(cls.build.cleanup)
        cls.driver = Path(cls.build.name) / "driver"
        subprocess.run([os.environ.get("CC", "cc"), "-Wall", "-Wextra", "-I", str(ROOT),
                        str(ROOT / "tests/dependency_driver.c"), str(ROOT / "dependencies.c"),
                        str(ROOT / "sha256.c"), "-o", str(cls.driver)], check=True)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="xasm-dependencies-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.source = self.root / "input.asm"
        self.source.write_text(".ORG $C000\nLDA #1\nRTS\nEND\n")
        self.manifest = self.root / "dependencies.json"
        self.output = self.root / "out.bin"

    def run_asm(self, *options, manifest=True, source=None):
        args = [str(XASM), "--pure-binary", str(source or self.source), "-o", str(self.output), *options]
        if manifest:
            args.append(f"--dependency-manifest={self.manifest}")
        return subprocess.run(args, cwd=self.root, capture_output=True, timeout=15)

    def read_manifest(self):
        data = json.loads(self.manifest.read_bytes())
        self.assertEqual((data["schema"], data["version"]), ("xasm-dependencies", "1"))
        for entry in data["inputs"]:
            payload = Path(entry["path"]).read_bytes()
            self.assertEqual(entry["size"], len(payload))
            self.assertEqual(entry["sha256"], hashlib.sha256(payload).hexdigest())
            self.assertTrue(Path(entry["path"]).is_absolute())
        for path in data["missing_paths"]:
            self.assertFalse(Path(path).exists())
        return data

    def test_source_binary_charmap_and_original_arguments(self):
        (self.root / "sub").mkdir()
        (self.root / "sub/part.inc").write_text('.INCBIN "data.bin"\nLDA #VALUE\n')
        (self.root / "sub/data.bin").write_bytes(bytes(range(256)))
        (self.root / "map.tbl").write_text("a=$42\n")
        self.source.write_text('.ORG $C000\n.CHARMAP "map.tbl"\n.INCSRC "sub/part.inc"\n.ASC "a"\nEND\n')
        xref = self.root / "xref.json"
        args = ["-DVALUE=3", f"--xref={xref}", "--xref-data=true", "--xref-instructions=true",
                f"--listing={self.root / 'listing.json'}", "--listing-format=json"]
        baseline = self.run_asm(*args, manifest=False)
        self.assertEqual(baseline.returncode, 0, baseline.stderr)
        expected = self.output.read_bytes()
        old = json.loads(xref.read_bytes())
        run = self.run_asm(*args)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(run.stderr, baseline.stderr)
        self.assertEqual(self.output.read_bytes(), expected)
        self.assertEqual(expected[-1], 0x42)
        new = json.loads(xref.read_bytes())
        old["build"].pop("timestamp_utc")
        new["build"].pop("timestamp_utc")
        self.assertEqual(new, old)
        data = self.read_manifest()
        entries = {Path(entry["path"]): entry for entry in data["inputs"]}
        for name, role in [("input.asm", "source"), ("sub/part.inc", "source"),
                           ("sub/data.bin", "binary"), ("map.tbl", "charmap")]:
            self.assertIn(role, entries[self.root / name]["roles"])
        self.assertIn("-DVALUE=3", data["invocation"]["argv"])
        self.assertEqual(data["invocation"]["cwd"], str(self.root))
        producer = [entry for entry in data["inputs"] if "producer" in entry["roles"]]
        self.assertEqual(len(producer), 1)
        self.assertEqual(producer[0]["sha256"], hashlib.sha256(XASM.read_bytes()).hexdigest())

    def test_include_lookup_records_only_actual_probes(self):
        for name in ["first", "second", "unused"]:
            (self.root / name).mkdir()
        (self.root / "second/part.inc").write_text("RTS\n")
        (self.root / "unused/part.inc").write_text("NOP\n")
        self.source.write_text('.ORG $C000\n.INCSRC "part.inc"\nEND\n')
        run = self.run_asm("-Ifirst", "-Isecond", "-Iunused")
        self.assertEqual(run.returncode, 0, run.stderr)
        data = self.read_manifest()
        self.assertEqual(set(data["missing_paths"]), {str(self.root / "part.inc"), str(self.root / "first/part.inc")})
        self.assertNotIn(str(self.root / "unused/part.inc"), [row["path"] for row in data["inputs"]])

    def test_inactive_parsed_includes_are_still_consumed_dependencies(self):
        (self.root / "unused.bin").write_bytes(b"data")
        self.source.write_text('.ORG $C000\n.IF 0\n.INCBIN "unused.bin"\n.ENDIF\nRTS\nEND\n')
        run = self.run_asm()
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(self.output.read_bytes(), b"\x60")
        self.assertIn(str(self.root / "unused.bin"), [row["path"] for row in self.read_manifest()["inputs"]])

    def test_comparison_input_is_hashed_not_generated_output(self):
        self.assertEqual(self.run_asm(manifest=False).returncode, 0)
        reference = self.root / "reference.bin"
        reference.write_bytes(self.output.read_bytes())
        run = self.run_asm(f"--compare={reference}")
        self.assertEqual(run.returncode, 0, run.stderr)
        entries = {row["path"]: row for row in self.read_manifest()["inputs"]}
        self.assertIn("comparison", entries[str(reference)]["roles"])
        self.assertNotIn(str(self.output), entries)

    def test_missing_comparison_reference_refuses_before_output(self):
        run = self.run_asm("--compare=missing.bin")
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertIn(b"could not read `missing.bin'", run.stderr)
        self.assertFalse(self.output.exists())
        self.assertFalse(self.manifest.exists())

    def test_sha256_known_vectors_and_block_boundaries(self):
        for payload in [b"", b"abc", b"a" * 1000000, *[bytes(range(n)) for n in [1, 55, 56, 63, 64, 65, 127, 128, 255]]]:
            path = self.root / "hash.bin"
            path.write_bytes(payload)
            for chunk in [1, 17, 16384]:
                with self.subTest(size=len(payload), chunk=chunk):
                    run = subprocess.run([str(self.driver), "hash", str(path), str(chunk)], capture_output=True, timeout=15)
                    self.assertEqual(run.returncode, 0, run.stderr)
                    self.assertEqual(run.stdout.decode().strip(), "snapshot=" + hashlib.sha256(payload).hexdigest())

    def mutate_after_snapshot(self, path, mutate, *, role=1, driver=None):
        proc = subprocess.Popen([str(driver or self.driver), str(self.manifest), str(role), str(path)],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE)
        self.addCleanup(lambda: proc.poll() is None and (proc.kill(), proc.wait()))
        self.assertEqual(proc.stdout.readline(), b"ready\n")
        mutate()
        stdout, stderr = proc.communicate(b"\n", timeout=15)
        return proc.returncode, stdout, stderr

    def test_unchanged_snapshot_can_publish(self):
        code, stdout, stderr = self.mutate_after_snapshot(self.source, lambda: None)
        self.assertEqual(code, 0, stderr)
        self.assertIn(hashlib.sha256(self.source.read_bytes()).hexdigest().encode(), stdout)
        self.read_manifest()

    def test_same_size_same_timestamp_mutation_refuses_each_input_role(self):
        for role in [1, 2, 4, 8, 16]:
            with self.subTest(role=role):
                self.source.write_bytes(b"original")
                before = self.source.stat()
                def mutate():
                    self.source.write_bytes(b"modified")
                    os.utime(self.source, ns=(before.st_atime_ns, before.st_mtime_ns))
                code, stdout, stderr = self.mutate_after_snapshot(self.source, mutate, role=role)
                self.assertEqual(code, 3, stderr)
                self.assertIn(b"input changed", stderr)
                self.assertIn(hashlib.sha256(b"original").hexdigest().encode(), stdout)
                self.assertFalse(self.manifest.exists())

    def test_new_shadowing_candidate_refuses(self):
        missing = self.root / "new.inc"
        code, _, stderr = self.mutate_after_snapshot(missing, lambda: missing.write_text("new"))
        self.assertEqual(code, 3, stderr)
        self.assertIn(b"input changed", stderr)
        self.assertFalse(self.manifest.exists())

    def test_unchanged_missing_probe_can_publish(self):
        missing = self.root / "absent.inc"
        code, _, stderr = self.mutate_after_snapshot(missing, lambda: None)
        self.assertEqual(code, 0, stderr)
        self.assertEqual(self.read_manifest()["missing_paths"], [str(missing)])

    def test_removed_input_refuses(self):
        code, _, stderr = self.mutate_after_snapshot(self.source, self.source.unlink)
        self.assertEqual(code, 3, stderr)
        self.assertIn(b"input changed", stderr)
        self.assertFalse(self.manifest.exists())

    def test_symlink_retarget_refuses(self):
        alternate = self.root / "alternate"
        alternate.write_text("changed")
        link = self.root / "alias"
        link.symlink_to(self.source)
        def retarget():
            link.unlink()
            link.symlink_to(alternate)
        code, _, stderr = self.mutate_after_snapshot(link, retarget)
        self.assertEqual(code, 3, stderr)
        self.assertIn(b"input changed", stderr)
        self.assertFalse(self.manifest.exists())

    def test_running_producer_path_replacement_refuses(self):
        executable = self.root / "driver"
        shutil.copy2(self.driver, executable)
        def replace():
            changed = self.root / "replacement"
            changed.write_bytes(executable.read_bytes() + b"changed")
            changed.replace(executable)
        code, _, stderr = self.mutate_after_snapshot(self.source, replace, driver=executable)
        self.assertEqual(code, 3, stderr)
        self.assertIn(b"input changed", stderr)
        self.assertFalse(self.manifest.exists())

    def test_manifest_cannot_alias_source_or_other_outputs(self):
        original = self.source.read_bytes()
        for kind in ["direct", "hardlink", "symlink", "output", "temporary", "xref"]:
            with self.subTest(kind=kind):
                manifest = self.root / f"{kind}.json"
                if kind == "direct": manifest = self.source
                elif kind == "hardlink": os.link(self.source, manifest)
                elif kind == "symlink": manifest.symlink_to(self.source)
                elif kind == "output": manifest = self.output
                elif kind == "temporary": manifest = Path(str(self.output) + ".tmp")
                options = [f"--dependency-manifest={manifest}"]
                if kind == "xref": options.append(f"--xref={manifest}")
                run = self.run_asm(*options, manifest=False)
                self.assertEqual(run.returncode, 3, run.stderr)
                self.assertIn(b"aliases", run.stderr)
                self.assertEqual(self.source.read_bytes(), original)

    def test_output_cannot_overwrite_input_with_manifest_enabled(self):
        original = self.source.read_bytes()
        run = self.run_asm("-o", str(self.source))
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertIn(b"aliases", run.stderr)
        self.assertEqual(self.source.read_bytes(), original)

    def test_output_cannot_overwrite_comparison_reference(self):
        reference = self.root / "reference.bin"
        reference.write_bytes(b"unchanged")
        run = self.run_asm("-o", str(reference), f"--compare={reference}")
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertIn(b"aliases", run.stderr)
        self.assertEqual(reference.read_bytes(), b"unchanged")

    def test_error_listing_cannot_overwrite_input(self):
        original = b"LDA #\n"
        self.source.write_bytes(original)
        run = self.run_asm(f"--listing={self.source}")
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertIn(b"aliases", run.stderr)
        self.assertEqual(self.source.read_bytes(), original)
        self.assertFalse(self.manifest.exists())

    def test_ignored_output_options_do_not_create_output_collisions(self):
        option = f"--data-consumers-output={self.source}"
        original = self.source.read_bytes()
        baseline = self.run_asm(option, manifest=False)
        run = self.run_asm(option)
        self.assertEqual(baseline.returncode, 0, baseline.stderr)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(run.stderr, baseline.stderr)
        self.assertEqual(self.source.read_bytes(), original)

    def test_help_lists_the_manifest_option(self):
        run = subprocess.run([str(XASM), "--help"], capture_output=True)
        self.assertEqual(run.returncode, 0)
        self.assertIn(b"--dependency-manifest=FILE", run.stdout)

    def test_failure_never_publishes_or_replaces_a_manifest(self):
        self.manifest.write_text("previous receipt")
        self.source.write_text("LDA #\n")
        run = self.run_asm()
        self.assertNotEqual(run.returncode, 0)
        self.assertEqual(self.manifest.read_text(), "previous receipt")
        self.source.write_text(".ORG 0\nRTS\nEND\n")
        run = self.run_asm("-o", str(self.root / "absent/out"))
        self.assertNotEqual(run.returncode, 0)
        self.assertEqual(self.manifest.read_text(), "previous receipt")

    def test_unwritable_manifest_path_refuses(self):
        self.manifest = self.root / "absent/receipt.json"
        run = self.run_asm()
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertIn(b"cannot create manifest output", run.stderr)
        self.assertFalse(self.manifest.exists())

    def test_nonregular_input_refuses_without_hanging(self):
        fifo = self.root / "fifo"
        os.mkfifo(fifo)
        run = self.run_asm(source=fifo)
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertIn(b"cannot snapshot regular input", run.stderr)

    def test_invalid_utf8_argument_refuses(self):
        run = self.run_asm(b"-DNAME=\xff")
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertIn(b"argument is unavailable or not UTF-8", run.stderr)

    def test_cli_refuses_unsupported_modes(self):
        for options in [["--dependency-manifest="], ["--xref=x", "--xref-format=csv"]]:
            run = self.run_asm(*options, manifest=bool(options[0] != "--dependency-manifest="))
            self.assertEqual(run.returncode, 2, run.stderr)
        run = subprocess.run([str(XASM), str(self.source), f"--dependency-manifest={self.manifest}"], capture_output=True)
        self.assertEqual(run.returncode, 2, run.stderr)
        self.assertFalse(self.manifest.exists())


if __name__ == "__main__":
    unittest.main()
