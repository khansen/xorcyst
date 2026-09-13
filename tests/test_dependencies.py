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
                        str(ROOT / "sha256.c"), str(ROOT / "output_file.c"), "-o", str(cls.driver),
                        *(["-framework", "CoreFoundation"] if sys.platform == "darwin" else [])], check=True)
        if sys.platform == "darwin":
            cls.pathconf_driver = Path(cls.build.name) / "pathconf-driver"
            subprocess.run([os.environ.get("CC", "cc"), "-Wall", "-Wextra", "-I", str(ROOT),
                            str(ROOT / "tests/dependency_driver.c"), str(ROOT / "tests/test_pathconf_faults.c"),
                            str(ROOT / "sha256.c"), str(ROOT / "output_file.c"), "-o", str(cls.pathconf_driver),
                            "-framework", "CoreFoundation"], check=True)

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

    def mutate_after_snapshot(self, path, mutate, *, role=1, driver=None, protect_only=False):
        mode = 'protect' if protect_only else str(self.manifest)
        proc = subprocess.Popen([str(driver or self.driver), mode, str(role), str(path)],
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

    def test_output_protection_reads_live_inputs_without_snapshots(self):
        self.source.write_bytes(b'original')
        code, stdout, stderr = self.mutate_after_snapshot(
            self.source, lambda: self.source.write_bytes(b'modified'), protect_only=True)
        self.assertEqual(code, 0, stderr)
        self.assertIn(hashlib.sha256(b'modified').hexdigest().encode(), stdout)
        self.assertFalse(self.manifest.exists())

    def test_output_protection_rejects_input_aliases_in_both_orders(self):
        hardlink = self.root / 'hardlink'
        symlink = self.root / 'symlink'
        os.link(self.source, hardlink)
        symlink.symlink_to(self.source)
        original = self.source.read_bytes()
        for kind in ('input', 'source', 'late-input'):
            for alias in (self.source, hardlink, symlink):
                with self.subTest(kind=kind, alias=alias.name):
                    run = subprocess.run([str(self.driver), 'protect-alias', kind, str(self.source), str(alias)],
                                         capture_output=True)
                    self.assertEqual(run.returncode, 3, run.stderr)
                    self.assertIn(b'aliases', run.stderr)
                    self.assertEqual(self.source.read_bytes(), original)
        self.assertFalse(self.manifest.exists())

    def test_output_protection_resolves_future_output_parents(self):
        directory_alias = self.root / 'directory-alias'
        directory_alias.symlink_to(self.root, target_is_directory=True)
        first = self.root / 'future.nl'
        for second in (str(self.root) + '/./future.nl', directory_alias / 'future.nl'):
            with self.subTest(second=str(second)):
                run = subprocess.run([str(self.driver), 'protect-alias', 'outputs', str(first), str(second)],
                                     capture_output=True)
                self.assertEqual(run.returncode, 3, run.stderr)
                self.assertIn(b'aliases', run.stderr)
                self.assertFalse(first.exists())
        other_directory = self.root / 'other'
        other_directory.mkdir()
        run = subprocess.run([str(self.driver), 'protect-alias', 'outputs', str(first),
                              str(other_directory / 'future.nl')], capture_output=True)
        self.assertEqual(run.returncode, 0, run.stderr)

    def test_future_outputs_follow_dangling_symlink_chains(self):
        target = self.root / 'future.nl'
        (self.root / 'sub').mkdir()
        (self.root / 'directory-alias').symlink_to(self.root / 'sub', target_is_directory=True)
        relative = self.root / 'relative'
        relative.symlink_to('directory-alias/../future.nl')
        absolute = self.root / 'absolute'
        absolute.symlink_to(relative)
        for alias in (relative, absolute):
            for first, second in ((target, alias), (alias, target)):
                with self.subTest(first=first.name, second=second.name):
                    run = subprocess.run([str(self.driver), 'protect-alias', 'outputs', str(first), str(second)],
                                         capture_output=True, timeout=5)
                    self.assertEqual(run.returncode, 3, run.stderr)
                    self.assertIn(b'aliases', run.stderr)
                    self.assertFalse(target.exists())
        other = self.root / 'other'
        other.symlink_to('different.nl')
        run = subprocess.run([str(self.driver), 'protect-alias', 'outputs', str(relative), str(other)],
                             capture_output=True, timeout=5)
        self.assertEqual(run.returncode, 0, run.stderr)

    @unittest.skipUnless(sys.platform == "darwin", "Darwin filesystem capabilities")
    def test_future_names_with_unknown_or_failed_case_sensitivity(self):
        for mode, first, second, expected in (
                ("unknown", "out.bin", "other.nl", 0),
                ("unknown", "out.bin", "OUT.BIN", 3),
                ("unknown", "Résumé.nl", "RE\u0301SUME\u0301.NL", 3),
                ("sensitive", "out.bin", "OUT.BIN", 0),
                ("insensitive", "out.bin", "OUT.BIN", 3),
                ("error", "out.bin", "other.nl", 3)):
            with self.subTest(mode=mode, first=first, second=second):
                run = subprocess.run([str(self.pathconf_driver), 'protect-alias', 'outputs',
                                      str(self.root / first), str(self.root / second)], capture_output=True,
                                     env={**os.environ, 'XASM_TEST_CASE_SENSITIVITY': mode}, timeout=5)
                self.assertIn(f'INJECT_PATHCONF {mode}'.encode(), run.stderr)
                self.assertEqual(run.returncode, expected, run.stderr)
                if mode == 'error':
                    self.assertIn(b'cannot determine output filesystem case sensitivity', run.stderr)
                else:
                    self.assertNotIn(b'cannot determine output filesystem case sensitivity', run.stderr)
                if expected == 3 and mode != 'error':
                    self.assertIn(b'aliases', run.stderr)
                self.assertFalse((self.root / first).exists())
                self.assertFalse((self.root / second).exists())

    def test_output_symlink_cycles_fail_without_hanging(self):
        first, second = self.root / 'first', self.root / 'second'
        first.symlink_to(second.name)
        second.symlink_to(first.name)
        run = subprocess.run([str(self.driver), 'protect-alias', 'outputs', str(first),
                              str(self.root / 'future.nl')], capture_output=True, timeout=5)
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertIn(b'resolv', run.stderr)
        self.assertTrue(first.is_symlink())
        self.assertTrue(second.is_symlink())

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

    def test_manifest_source_alias_preserves_all_outputs(self):
        ram = self.root / 'game.nes.ram.nl'
        bank = self.root / 'game.nes.0.nl'
        xref = self.root / 'xref.json'
        listing = self.root / 'listing.txt'
        previous = {self.source: self.source.read_bytes()}
        for path in (self.output, ram, bank, xref, listing):
            previous[path] = b'previous ' + path.name.encode()
            path.write_bytes(previous[path])
        run = self.run_asm(f'--dependency-manifest={self.source}', f'--xref={xref}',
                           f'--listing={listing}', f'--fceux-nl-rom-prefix={self.root}/game.nes.',
                           f'--fceux-nl-ram-output={ram}', manifest=False)
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertIn(b'manifest output aliases an input', run.stderr)
        self.assertEqual({path: path.read_bytes() for path in previous}, previous)

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

    def test_outputs_cannot_overwrite_later_diagnostic_source_reads(self):
        (self.root / "sub").mkdir()
        self.source.write_text('.ORG $C000\n.INCSRC "sub/outer.inc"\nEND\n')
        (self.root / "sub/outer.inc").write_text('.INCSRC "inner.inc"\n')
        (self.root / "sub/inner.inc").write_text('RTS\n')
        diagnostic = self.root / "inner.inc"
        original = b"original diagnostic source\n"
        diagnostic.write_bytes(original)
        control = self.run_asm("--listing=listing.txt")
        self.assertEqual(control.returncode, 0, control.stderr)
        entries = {row["path"]: row for row in self.read_manifest()["inputs"]}
        self.assertEqual(entries[str(diagnostic)]["roles"], ["analysis_source"])
        self.assertEqual(diagnostic.read_bytes(), original)
        previous = self.manifest.read_bytes()
        for options in [["--listing=inner.inc"], ["-o", "inner.inc", "--listing=listing.txt"],
                        ["--xref=inner.inc", "--listing=listing.txt"],
                        ["--dependency-manifest=inner.inc"]]:
            with self.subTest(options=options):
                run = self.run_asm(*options, manifest=not options[0].startswith("--dependency-manifest="))
                self.assertEqual(run.returncode, 3, run.stderr)
                self.assertIn(b"aliases", run.stderr)
                self.assertEqual(diagnostic.read_bytes(), original)
                self.assertEqual(self.manifest.read_bytes(), previous)

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
        self.assertIn(b"could not write output", run.stderr)
        self.assertIn(os.fsencode(self.manifest), run.stderr)
        self.assertEqual(sum(line.startswith(b'error:') for line in run.stderr.splitlines()), 1)
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
