#!/usr/bin/env python3
"""Omitted initializer fields retain their positions and emit zero padding."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


XASM = Path(sys.argv.pop(1)).resolve()
XLNK = Path(os.environ.get("XLNK", Path(__file__).resolve().parents[1] / "xlnk")).resolve()

TYPES = """.record Flags padding:1, enabled:1, index:6
.record Quads first:2, second:2, third:2, fourth:2
.struc Layout
prefix .byte
pair .word
payload .dsb 3
suffix .byte
.ends
.union Choice
small .byte
medium .word
large .dword
bits .Flags
.ends
.struc Nested
head .byte
inner .Layout
choice .Choice
bits .Flags
tail .word
.ends
"""


class FieldInitializers(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="xasm-field-initializers-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def assemble(self, source, *, pure_binary=True, options=(), fresh_output=False):
        assembly = self.root / "input.asm"
        binary = self.root / ("output.bin" if pure_binary else "output.o")
        # Failure tests may seed an output file to verify it survives an error.
        if fresh_output and binary.exists():
            binary.unlink()
        assembly.write_text(source)
        result = subprocess.run([str(XASM), *(["--pure-binary"] if pure_binary else []),
                                 *options, str(assembly), "-o", str(binary)],
                                capture_output=True, timeout=10)
        return result, binary

    def link(self, object_file, origin=0):
        script, binary = self.root / "link.script", self.root / "linked.bin"
        if binary.exists():
            binary.unlink()
        script.write_text(f"link{{file={object_file.name},origin=${origin:04X}}}\n")
        result = subprocess.run([str(XLNK), str(script), "-o", str(binary)],
                                cwd=self.root, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        return binary

    def assert_bytes(self, body, expected, declarations=""):
        # Repeated macro expansion also exercises cloning of the NULL_NODE slots.
        for macro in (False, True):
            for pure_binary in (False, True):
                with self.subTest(macro=macro, pure_binary=pure_binary):
                    source = (f".macro Emit\n{body}.endm\nEmit\nEmit\n" if macro else body)
                    result, binary = self.assemble(TYPES + declarations + source, pure_binary=pure_binary)
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    if not pure_binary:
                        binary = self.link(binary)
                    self.assertEqual(binary.read_bytes(), expected * (2 if macro else 1))

    def test_invalid_deferred_storage_counts_preserve_output(self):
        declarations = (".struc Point\npx .byte\npy .byte\n.ends\n"
                        ".struc Box\nitems .Point[Count]\ntail .byte[4]\n.ends\n"
                        ".struc BufferFields\nitems .byte[Count]\ntail .byte[4]\n.ends\n")
        bodies = ((".dsb Count\n.dsb 4\n", (0, -1), b"operand out of range"),
                  ("buf .word[Count]\n", (0, -1), b"operand out of range"),
                  (".Box {,{1,2,3,4}}\n", (0, -1), b"operand out of range"),
                  (".Box {}\n", (0, -1), b"operand out of range"),
                  (".BufferFields {{1},{1,2,3,4}}\n", (0, -1), b"initializer for field `items' exceeds field size"),
                  (".BufferFields {,{1,2,3,4}}\n", (-1,), b"operand out of range"),
                  (".BufferFields {}\n", (-1,), b"operand out of range"),
                  ("buf .Point[Count]\n", (0, -1), b"operand out of range"))
        for body, invalid_counts, message in bodies:
            for value in (*invalid_counts, 2):
                for pure_binary in (False, True):
                    for debug in (False, True):
                        with self.subTest(body=body, value=value, binary=pure_binary, debug=debug):
                            target = self.root / ("output.bin" if pure_binary else "output.o")
                            target.write_bytes(b"previous output")
                            source = declarations + body + ".byte $5A\nCount = " + str(value) + "\n"
                            result, output = self.assemble(source, pure_binary=pure_binary,
                                                          options=("--debug",) if debug else ())
                            if value == 2:
                                self.assertEqual(result.returncode, 0, result.stderr.decode())
                                if not pure_binary: output = self.link(output)
                                self.assertTrue(output.read_bytes().endswith(b"Z"))
                            else:
                                self.assertEqual(result.returncode, 1, result.stderr.decode())
                                self.assertEqual(result.stderr.count(b"error:"), 1, result.stderr.decode())
                                self.assertIn(message, result.stderr)
                                self.assertEqual(output.read_bytes(), b"previous output")

    def test_deferred_builtin_padding_bytes_and_following_addresses(self):
        for kind, width in (("byte", 1), ("char", 1), ("word", 2), ("dword", 4)):
            for count in (1, 2):
                for initializer in ("{1}", "", "{}"):
                    declarations = f".struc S\nitems .{kind}[Count]\ntail .byte\n.ends\n"
                    value = 65 if kind == "char" else 1
                    expected = ((bytes([value]) + bytes(width - 1)) if initializer == "{1}" else bytes(width))
                    expected += bytes(width * (count - 1)) + b"Z"
                    for pure_binary in (False, True):
                        for debug in (False, True):
                            with self.subTest(kind=kind, count=count, init=initializer,
                                              binary=pure_binary, debug=debug):
                                field_init = '"A"' if kind == "char" and initializer == "{1}" else initializer
                                source = declarations + f".S {{{field_init},$5A}}\nAfter: .word After\nCount = {count}\n"
                                result, output = self.assemble(source, pure_binary=pure_binary,
                                                              options=("--debug",) if debug else ())
                                self.assertEqual(result.returncode, 0, result.stderr.decode())
                                origin = 0 if pure_binary else 0x8000
                                if not pure_binary: output = self.link(output, origin)
                                self.assertEqual(output.read_bytes(), expected + (origin + len(expected)).to_bytes(2, "little"))

    def test_zero_padding_is_distinct_from_invalid_storage(self):
        sources = (".struc Empty\n.ends\nbuf .Empty[Count]\n",
                   ".struc S\nitems .byte[Count-1]\n.ends\n.S {{}}\n")
        for source in sources:
            for pure_binary in (False, True):
                with self.subTest(source=source, binary=pure_binary):
                    result, output = self.assemble(source + ".byte $5A\nCount = 1\n", pure_binary=pure_binary)
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    if not pure_binary: output = self.link(output)
                    self.assertEqual(output.read_bytes(), b"Z")

    def test_deferred_storage_validation_in_expansions_and_data_segment(self):
        for wrapper in ("{}", ".macro Emit\n{}\n.endm\nEmit\n", ".rept 2\n{}\n.endm\n", ".dataseg\n{}"):
            for value in (0, -1, 65536):
                with self.subTest(wrapper=wrapper, value=value):
                    result, _ = self.assemble(wrapper.format(".dsb Count\n.dsb 4\n") + f"Count = {value}\n")
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    self.assertEqual(result.stderr.count(b"error:"), 2 if ".rept" in wrapper else 1,
                                     result.stderr.decode())

    def test_nested_omitted_fields_validate_each_count(self):
        declarations = (".struc Inner\nitems .byte[Count]\ntail .byte[4]\n.ends\n"
                        ".struc Outer\ninner .Inner[2]\ntail .byte\n.ends\n")
        for count in (-1, 0, 1, 2):
            for body in (".Outer {}\n", ".Outer {,$5A}\n", "buffer .Outer\n"):
                for pure_binary in (False, True):
                    with self.subTest(count=count, body=body, binary=pure_binary):
                        result, output = self.assemble(declarations + body + f"Count={count}\n",
                                                       pure_binary=pure_binary)
                        if count < 0:
                            self.assertEqual(result.returncode, 1, result.stderr.decode())
                            self.assertIn(b"operand out of range", result.stderr)
                        else:
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            if not pure_binary: output = self.link(output)
                            self.assertEqual(output.read_bytes(), bytes(2*(count+4)) + (b"Z" if "$5A" in body else b"\0"))

    def test_zero_length_omitted_fields_preserve_storage(self):
        for kind in ("byte", "char", "word", "dword", "Empty", "Point"):
            for definition in ("literal", "before", "after_declaration", "deferred"):
                if kind in ("Empty", "Point") and definition == "deferred":
                    continue  # Deferred zero structure-array counts remain unsupported.
                count = "0" if definition == "literal" else "Zero"
                declarations = (".struc Empty\n.ends\n.struc Point\npx .byte\npy .byte\n.ends\n"
                                ".struc Packet\nlen .byte\n"
                                f"payload .{kind}[{count}]\n.ends\n")
                if definition == "before": declarations = "Zero=0\n" + declarations
                if definition == "after_declaration": declarations += "Zero=0\n"
                for data_segment in (False, True):
                    body = (".dataseg\ninst .Packet[2]\n.codeseg\n" if data_segment else
                            ".Packet\ninst .Packet[2]\n")
                    body += ".byte $5A\nlda #sizeof Packet\nAfter: .word After\n"
                    if definition == "deferred": body += "Zero=0\n"
                    for pure in (False, True):
                        for debug in (False, True):
                            with self.subTest(kind=kind, definition=definition, data=data_segment,
                                              pure=pure, debug=debug):
                                result, output = self.assemble(declarations + body, pure_binary=pure,
                                                              options=("--debug",) if debug else ())
                                self.assertEqual(result.returncode, 0, result.stderr.decode())
                                expected = (b"" if data_segment else bytes(3)) + bytes.fromhex("5A A9 01")
                                origin = 0 if pure else 0x8000
                                expected += (origin + len(expected)).to_bytes(2, "little")
                                if not pure: output = self.link(output, origin)
                                self.assertEqual(output.read_bytes(), expected)

    def test_zero_length_fields_in_partial_initializers(self):
        for count in ("0", "Zero"):
            declarations = (f".struc S\nhead .byte\nitems .byte[{count}]\ntail .byte\n.ends\n")
            for body in (".S {$11}\n", ".S {$11,}\n", ".S {$11,,0}\n"):
                for pure in (False, True):
                    with self.subTest(count=count, body=body, pure=pure):
                        result, output = self.assemble(declarations + body + "After: .word After\nZero=0\n",
                                                      pure_binary=pure)
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        if not pure: output = self.link(output, 0x8000)
                        self.assertEqual(output.read_bytes(), bytes.fromhex("11 00 02") + (b"\0" if pure else b"\x80"))

    def test_cyclic_direct_storage_has_storage_diagnostic(self):
        for pure in (False, True):
            with self.subTest(pure=pure):
                result, _ = self.assemble(".dsb Value\nValue .equ Value\n", pure_binary=pure)
                self.assertEqual(result.returncode, 1, result.stderr.decode())
                self.assertEqual(result.stderr.count(b"error:"), 1, result.stderr.decode())
                self.assertIn(b"circular definition in storage size", result.stderr)

    def test_zero_length_nonempty_structure_fields(self):
        for count in ("0", "Count"):
            for deferred in (False, True):
                declarations = (".struc Point\npx .byte\npy .byte\n.ends\n"
                                f".struc S\nhead .byte\npoints .Point[{count}]\ntail .byte\n.ends\n")
                for body in (".S {$11}\n", ".S {$11,}\n", ".S {$11,,0}\n", "inst .S[1]\n"):
                    for pure in (False, True):
                        for debug in (False, True):
                            with self.subTest(count=count, deferred=deferred, body=body, pure=pure, debug=debug):
                                source = declarations + ("" if deferred else "Count=0\n")
                                source += body + "After: .word After\n" + ("Count=0\n" if deferred else "")
                                target = self.root / ("output.bin" if pure else "output.o")
                                target.write_bytes(b"previous output")
                                result, output = self.assemble(source, pure_binary=pure,
                                                              options=("--debug",) if debug else ())
                                if count == "Count" and deferred and not body.startswith("inst"):
                                    # One element is expanded before its count is known; this
                                    # documented limit must fail instead of emitting that element.
                                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                                    self.assertIn(b"operand out of range", result.stderr)
                                    self.assertEqual(output.read_bytes(), b"previous output")
                                else:
                                    # Uninitialized storage expands in pass 2, after this
                                    # definition is available, so it can omit the element.
                                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                                    if not pure: output = self.link(output, 0x8000)
                                    expected = bytes([0 if body.startswith("inst") else 0x11, 0, 2,
                                                      0 if pure else 0x80])
                                    self.assertEqual(output.read_bytes(), expected)

    def test_invalid_dotted_field_counts_preserve_output(self):
        declarations = (".enum Kind\nOne\n.ende\n"
                        ".struc Point\npx .byte\npy .byte\n.ends\ninst .Point\n")
        references = (("Nope.fx", b"unknown symbol `Nope'"),
                      ("Kind.One", b"`Kind' is not a structure"),
                      ("Point.py", b"`Point' is not a structure"),
                      ("inst.missing", b"`missing' is not a member of `inst'"),
                      ("inst.px.missing", b"member `px' of `inst' is not a structure"))
        for reference, message in references:
            for kind in ("byte", "Point"):
                for body in (".S {$11}\n", ".S {$11,{1}}\n", "buf .S[1]\n"):
                    for pure in (False, True):
                        for analysis in (False, True):
                            with self.subTest(reference=reference, kind=kind, body=body, pure=pure, analysis=analysis):
                                source = declarations + f".struc S\nhd .byte\nitems .{kind}[{reference}]\ntl .byte\n.ends\n" + body
                                target = self.root / ("output.bin" if pure else "output.o")
                                target.write_bytes(b"previous output")
                                options = ("--debug", f"--listing={self.root / 'listing.json'}",
                                           "--listing-format=json", f"--xref={self.root / 'xref.json'}") if analysis else ()
                                result, output = self.assemble(source, pure_binary=pure, options=options)
                                self.assertEqual(result.returncode, 1, result.stderr.decode())
                                self.assertIn(message, result.stderr)
                                self.assertEqual(output.read_bytes(), b"previous output")

    def test_reported_record_reproduction(self):
        self.assert_bytes(".Flags {, 1, 0}\n", b"\x40")

    def test_forward_dotted_alias_counts_fail_without_replacing_output(self):
        declarations = (".struc S\nfa .byte\nfb .byte\n.ends\ninst .S\nv: .S {1,2}\n"
                        ".struc Layout\nhead .byte\nitems .byte[Alias]\n.ends\n"
                        ".struc NestedCount\nhead .byte\nitems .S[Alias]\n.ends\n")
        references = (("Nope.fx", b"unknown symbol `Nope'"),
                      ("inst.missing", b"`missing' is not a member of `inst'"),
                      ("inst.fa.deep", b"member `fa' of `inst' is not a structure"),
                      ("v.fb", b"`v' is not a structure"))
        bodies = (".dsb Alias\n", "buf .byte[Alias]\n",
                  ".dataseg\nbuf .byte[Alias]\n.codeseg\n", ".Layout {$11}\n",
                  ".Layout {$11,{1}}\n", ".NestedCount {$11}\n", "buf .NestedCount\n")
        for reference, diagnostic in references:
            for body in bodies:
                for pure in (False, True):
                    for debug in (False, True):
                        with self.subTest(reference=reference, body=body, pure=pure, debug=debug):
                            target = self.root / ("output.bin" if pure else "output.o")
                            target.write_bytes(b"previous output")
                            result, output = self.assemble(declarations + body + f".byte $77\nAlias .equ {reference}\n",
                                                          pure_binary=pure, options=("--debug",) if debug else ())
                            self.assertEqual(result.returncode, 1, result.stderr.decode())
                            self.assertIn(diagnostic, result.stderr)
                            self.assertNotIn(b"put_expr_recursive", result.stderr)
                            self.assertEqual(output.read_bytes(), b"previous output")

    def test_forward_invalid_dotted_operands_fail_on_first_branch(self):
        declarations = ".struc S\nfa .byte\nfb .byte\n.ends\nv: .S {1,2}\n"
        for reference, diagnostic in (("v.fb", b"`v' is not a structure"),
                                      ("Nope.fx", b"unknown symbol `Nope'"),
                                      ("Val.fx", b"is not a structure")):
            for body in ("lda Alias\n", ".word Alias\n", ".dsb Alias\n"):
                for pure in (False, True):
                    for analysis in (False, True):
                        with self.subTest(reference=reference, body=body, pure=pure, analysis=analysis):
                            target = self.root / ("output.bin" if pure else "output.o")
                            target.write_bytes(b"previous output")
                            options = ("--debug", f"--listing={self.root / 'late.json'}",
                                       "--listing-format=json", f"--xref={self.root / 'late.xref'}") if analysis else ()
                            result, output = self.assemble(declarations + body + f"Alias .equ {reference}\nVal=1\n",
                                                          pure_binary=pure, options=options)
                            self.assertEqual(result.returncode, 1, result.stderr.decode())
                            self.assertIn(diagnostic, result.stderr)
                            self.assertEqual(output.read_bytes(), b"previous output")

    def test_unresolved_alias_storage_counts_fail_cleanly(self):
        for value in ("Undefined", '"text"'):
            for body in (".dsb Alias\n", ".dataseg\nbuf .byte[Alias]\n.codeseg\n"):
                for pure in (False, True):
                    with self.subTest(value=value, body=body, pure=pure):
                        target = self.root / ("output.bin" if pure else "output.o")
                        target.write_bytes(b"previous output")
                        result, output = self.assemble(body + f".byte $77\nAlias .equ {value}\n", pure_binary=pure)
                        self.assertEqual(result.returncode, 1, result.stderr.decode())
                        self.assertIn(b"unknown symbol `Undefined'" if value == "Undefined" else
                                      b"storage size expression does not evaluate to integer literal", result.stderr)
                        self.assertEqual(output.read_bytes(), b"previous output")

    def test_address_dependent_storage_remains_supported(self):
        for body, expected in ((".org $8000\n.dsb $8002-$\n.byte $77\n", b"\0\0w"),
                               (".dsb 1+($-$)\n.byte $77\n", b"\0w"),
                               ("Begin: .byte 1\nLast: .dsb Last-Begin\n.byte $77\n", b"\1\0w")):
            for debug in (False, True):
                with self.subTest(body=body, debug=debug):
                    result, output = self.assemble(body, options=("--debug",) if debug else ())
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    self.assertEqual(output.read_bytes(), expected)
        provider = self.root / "provider.asm"
        provider.write_text(".public Count\nCount=2\n")
        built = subprocess.run([str(XASM), str(provider), "-o", str(self.root / "provider.o")],
                               capture_output=True, timeout=10)
        self.assertEqual(built.returncode, 0, built.stderr.decode())
        for expression in ("Count", "Count+1", "Alias"):
            with self.subTest(expression=expression):
                result, output = self.assemble(f".extrn Count:label\n.dsb {expression}\n.byte $77\nAlias .equ Count\n",
                                              pure_binary=False)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                script = self.root / "external.script"
                script.write_text(f"link{{file={output.name},origin=$8000}}\nlink{{file=provider.o,origin=$9000}}\n")
                linked = subprocess.run([str(XLNK), str(script), "-o", "external.bin"], cwd=self.root,
                                        capture_output=True, timeout=10)
                self.assertEqual(linked.returncode, 0, linked.stderr.decode())
                self.assertEqual((self.root / "external.bin").read_bytes(), bytes(3 if expression == "Count+1" else 2) + b"w")

    def test_forward_count_aliases_retain_labels_without_analysis(self):
        cases = (
            ("Begin: .byte 1\nLast: .dsb Alias\n.byte $77\nAlias .equ Last-Begin\n", "01 00 77"),
            ("Lab: .byte 1\n.dsb Alias\n.byte $77\nAlias .equ Lab+2\n", "01 00 00 77"),
            ("Lab: .byte 1\n.dsb Alias\n.byte $77\nAlias .equ Mid\nMid .equ Lab+2\n", "01 00 00 77"),
            (".struc S\nitems .byte[Alias]\n.ends\nBegin: .byte 1\nLast: .S {}\n"
             ".byte $77\nAlias .equ Last-Begin\n", "01 00 77"),
            (".struc S\nitems .byte[Alias]\n.ends\nBegin: .byte 1\nLast: .S {{2}}\n"
             ".byte $77\nAlias .equ Last-Begin+1\n", "01 02 00 77"),
        )
        for case, (source, expected) in enumerate(cases):
            for proc in (False, True):
                body = ".proc Routine\n" + source + ".endp\n" if proc else source
                for pure in (False, True):
                    for debug in (False, True):
                        for analysis in ("none", "listing", "xref"):
                            with self.subTest(case=case, proc=proc, pure=pure, debug=debug, analysis=analysis):
                                path = self.root / f"analysis-{case}-{proc}-{pure}-{debug}-{analysis}"
                                options = ("--debug",) if debug else ()
                                if analysis == "listing":
                                    options += (f"--listing={path}", "--listing-format=json")
                                elif analysis == "xref":
                                    options += (f"--xref={path}",)
                                result, output = self.assemble(body, pure_binary=pure, options=options,
                                                               fresh_output=True)
                                self.assertEqual(result.returncode, 0, result.stderr.decode())
                                self.assertNotIn(b"defined but not used", result.stderr)
                                if not pure:
                                    output = self.link(output)
                                self.assertEqual(output.read_bytes(), bytes.fromhex(expected))
                                if analysis != "none":
                                    self.assertTrue(path.is_file())

    def test_count_aliases_do_not_retain_unrelated_labels(self):
        cases = (
            ("Begin: .byte 1\nLast: .dsb Alias\n.byte $77\nUnused: .byte 5\n"
             "Alias .equ Last-Begin+defined(Unused)\n", "01 00 00 77 05", ("Unused",), (False, True)),
            (".struc S\nfa .byte\nfb .byte\n.ends\nfb: .byte 9\n"
             "Begin: .byte 1\nLast: .dsb Alias\n.byte $77\nAlias .equ S::fb+Last-Begin\n",
             "09 01 00 00 77", ("fb",), (False, True)),
            (".enum E\nfb\nfc\n.ende\nfc: .byte 9\n"
             "Begin: .byte 1\nLast: .dsb Alias\n.byte $77\nAlias .equ E::fc+Last-Begin\n",
             "09 01 00 00 77", ("fc",), (False, True)),
            ("Begin: .byte 1\nLast: .byte 2\nAlias .equ Last-Begin\n",
             "01 02", ("Begin", "Last"), (False, True)),
            (".macro Emit\n.dsb Alias\n.endm\nBegin: .byte 1\nLast: .byte 2\nAlias .equ Last-Begin\n",
             "01 02", ("Begin", "Last"), (False, True)),
            ("Begin: .byte 1\nLast: .byte 2\n.if 0\n.dsb Alias\n.endif\nAlias .equ Last-Begin\n",
             "01 02", ("Begin", "Last"), (False, True)),
            ("Begin: .byte 1\nLast: .dsb Alias\n.byte $77\nAlias .equ Last-Begin\n"
             ".undef Alias\nAlias .equ 3\n", "01 00 00 00 77", ("Begin", "Last"), (False, True)),
            # Reach the walk's guards: types declared after the alias, defined()
            # in a count, and an alias whose dotted base is declared later.
            ("Begin: .byte 1\nLast: .dsb Alias\n.byte $77\nAlias .equ S::fb+Last-Begin\n"
             ".struc S\nfa .byte\nfb .byte\n.ends\nfb: .byte 9\n", "01 00 00 77 09", ("fb",), (False, True)),
            ("Begin: .byte 1\nLast: .dsb Alias\n.byte $77\nAlias .equ E::fc+Last-Begin\n"
             ".enum E\nfb\nfc\n.ende\nfc: .byte 9\n", "01 00 00 77 09", ("fc",), (False, True)),
            ("Begin: .byte 1\nLast: .dsb Last-Begin+defined(Unused)\n.byte $77\nUnused: .byte 5\n",
             "01 00 00 77 05", ("Unused",), (False, True)),
            # Pure binary does not resolve data-variable addresses; both dotted
            # cases therefore check the member-address difference via the linker.
            (".struc S\nfa .byte\nfb .byte\n.ends\nAlias .equ v.fb-v\nv .S {1,2}\nfb: .byte 9\n"
             ".dsb Alias\n.byte $77\n", "01 02 09 00 77", ("fb",), (False,)),
            (".struc S\nfa .byte\nfb .byte\n.ends\nv .S {1,2}\nfb: .byte 9\n"
             ".dsb Alias\n.byte $77\nAlias .equ v.fb-v\n", "01 02 09 00 77", ("fb",), (False,)),
        )
        for case, (source, expected, unused, modes) in enumerate(cases):
            for pure in modes:
                for debug in (False, True):
                    for analysis in ("none", "listing", "xref"):
                        with self.subTest(case=case, pure=pure, debug=debug, analysis=analysis):
                            path = self.root / f"unused-{case}-{pure}-{debug}-{analysis}"
                            options = ("--debug",) if debug else ()
                            if analysis == "listing":
                                options += (f"--listing={path}", "--listing-format=json")
                            elif analysis == "xref":
                                options += (f"--xref={path}",)
                            result, output = self.assemble(source, pure_binary=pure, options=options,
                                                           fresh_output=True)
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            messages = [line.split(": warning: ", 1)[1]
                                        for line in result.stderr.decode().splitlines() if ": warning: " in line]
                            self.assertCountEqual(messages, [f"`{name}' defined but not used" for name in unused])
                            if not pure:
                                output = self.link(output)
                            self.assertEqual(output.read_bytes(), bytes.fromhex(expected))
                            if analysis != "none":
                                self.assertTrue(path.is_file())

    def test_record_omission_positions(self):
        # All subsets cover leading, middle, trailing, consecutive and all-omitted fields.
        for present in range(16):
            with self.subTest(present=present):
                values = ("1", "2", "3", "1")
                fields = [value if present & (1 << i) else "" for i, value in enumerate(values)]
                expected = sum(value for i, value in enumerate((0x40, 0x20, 0x0C, 0x01))
                               if present & (1 << i))
                self.assert_bytes(".Quads {" + ",".join(fields) + "}\n", bytes([expected]))

    def test_structure_omission_positions(self):
        for present in range(16):
            with self.subTest(present=present):
                values = ("$11", "$3322", '"AZ"', "$44")
                parts = (b"\x11", b"\x22\x33", b"AZ\0", b"\x44")
                fields = [value if present & (1 << i) else "" for i, value in enumerate(values)]
                expected = b"".join(part if present & (1 << i) else bytes(len(part))
                                    for i, part in enumerate(parts))
                self.assert_bytes(".Layout {" + ",".join(fields) + "}\n", expected)

    def test_union_field_selection(self):
        cases = (
            ("{$11,,,}", "11 00 00 00"),
            ("{,$3322,,}", "22 33 00 00"),
            ("{,,$77665544,}", "44 55 66 77"),
            ("{,,,{,1,0}}", "40 00 00 00"),
            ("{,$3322}", "22 33 00 00"),
            ("{,,$77665544}", "44 55 66 77"),
            ("{,,,}", "00 00 00 00"),
        )
        for initializer, expected in cases:
            with self.subTest(initializer=initializer):
                self.assert_bytes(f".Choice {initializer}\n", bytes.fromhex(expected))

    def test_empty_and_partial_initializers(self):
        cases = (
            (".Flags {}\n", "00"),
            (".Layout {}\n", "00 00 00 00 00 00 00"),
            (".Choice {}\n", "00 00 00 00"),
            (".Flags {1}\n", "80"),
            (".Flags {1,}\n", "80"),
            (".Flags {,}\n", "00"),
            (".Layout {$11}\n", "11 00 00 00 00 00 00"),
            (".Layout {$11,}\n", "11 00 00 00 00 00 00"),
            (".Layout {,}\n", "00 00 00 00 00 00 00"),
            (".Choice {,}\n", "00 00 00 00"),
        )
        for body, expected in cases:
            with self.subTest(body=body):
                self.assert_bytes(body, bytes.fromhex(expected))

    def test_nested_initializers(self):
        cases = (
            ('.Nested {, {,$3322,,}, {,,,{,1,5}}, {1,,7}, $9988}\n',
             "00 00 22 33 00 00 00 00 45 00 00 00 87 88 99"),
            ('.Nested {$11, {}, {}, {}, $9988}\n',
             "11 00 00 00 00 00 00 00 00 00 00 00 00 88 99"),
            ('.Nested {$11, , , , $9988}\n',
             "11 00 00 00 00 00 00 00 00 00 00 00 00 88 99"),
            ('.Nested {, {,,{1,2},$44}, , ,}\n',
             "00 00 00 00 01 02 00 44 00 00 00 00 00 00 00"),
            ('.Nested {}\n',
             "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"),
        )
        for body, expected in cases:
            with self.subTest(body=body):
                self.assert_bytes(body, bytes.fromhex(expected))

    def test_multiple_initializers(self):
        self.assert_bytes('.Flags {,1,0}, {}, {1,,7}, {,,}\n', bytes.fromhex("40 00 87 00"))
        self.assert_bytes('.Layout {}, {,$3322,,}, {$11,,"AZ",}\n',
                          bytes.fromhex("00 00 00 00 00 00 00"
                                        "00 22 33 00 00 00 00"
                                        "11 00 00 41 5A 00 00"))

    def test_empty_structures_do_not_skip_following_statements(self):
        declarations = ".struc Empty\n.ends\n"
        for body in (".Empty {}\n.Flags {,1,0}\n",
                     ".Empty {}, {}\n.Flags {,1,0}\n",
                     ".Empty {}\n.Empty {}\n.Flags {,1,0}\n",
                     '.Empty {}\n.char "@"\n',
                     '.Empty {}\n.rept 2\n.Flags {,1,0}\n.endm\n'):
            with self.subTest(body=body):
                expected = b"\x40\x40" if ".rept" in body else b"\x40"
                self.assert_bytes(body, expected, declarations)

    def test_empty_conditionals_do_not_skip_following_statements(self):
        declarations = ".define Defined\n"
        cases = (".if 1\n.endif\n", ".if 0\n.else\n.endif\n",
                 ".if 0\n.elif 1\n.endif\n", ".if 0\n.endif\n",
                 ".ifdef Defined\n.endif\n", ".ifndef Missing\n.endif\n",
                 ".ifdef Missing\n.else\n.endif\n", ".ifndef Defined\n.else\n.endif\n",
                 ".ifdef Missing\n.endif\n", ".ifndef Defined\n.endif\n",
                 ".if 1\n.ifdef Defined\n.endif\n.endif\n")
        for conditional in cases:
            with self.subTest(conditional=conditional):
                self.assert_bytes(conditional + ".Flags {,1,0}\n", b"\x40", declarations)
            for wrapper in (conditional, ".macro Empty\n" + conditional + ".endm\nEmpty\n",
                            ".rept 2\n" + conditional + ".endm\n"):
                for pure_binary in (False, True):
                    with self.subTest(wrapper=wrapper, pure_binary=pure_binary):
                        source = TYPES + declarations + (".org $8000\n" if pure_binary else "")
                        source += wrapper + ".Flags {,1,0}\nAfter: nop\njmp After\n"
                        result, binary = self.assemble(source, pure_binary=pure_binary)
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        if not pure_binary:
                            binary = self.link(binary, origin=0x8000)
                        self.assertEqual(binary.read_bytes(), bytes.fromhex("40 EA 4C 01 80"))

    def test_omitted_user_type_arrays_and_enum(self):
        declarations = """.enum Mode
off
on
.ende
.struc Arrays
layouts .Layout[2]
choices .Choice[2]
flags .Flags[3]
mode .Mode
tail .byte
.ends
"""
        self.assert_bytes(".Arrays {,,,,$5A}\n", bytes(26) + b"\x5A", declarations)

    def test_anonymous_union_padding(self):
        declarations = """.struc Inline
head .byte
.union
small .byte
wide .word
.ends
tail .byte
.ends
"""
        for initializer, expected in (
                ("{}", "00 00 00 00"),
                ("{$11,}", "11 00 00 00"),
                ("{$11,,}", "11 00 00 00"),
                ("{$11,,$44}", "11 00 00 44"),
                ("{,{},}", "00 00 00 00"),
                ("{,{,$3322},$44}", "00 22 33 44")):
            with self.subTest(initializer=initializer):
                self.assert_bytes(f".Inline {initializer}\n", bytes.fromhex(expected), declarations)

    def test_initialized_user_type_array_fields(self):
        declarations = """.enum Mode
off
on
.ende
.struc Arrays
layouts .Layout[2]
choices .Choice[2]
flags .Flags[3]
modes .Mode[3]
tail .byte
.ends
"""
        expected = (bytes.fromhex("11 22 33 00 00 00 44") + bytes(7)
                    + bytes.fromhex("55 66 00 00") + bytes(4)
                    + b"\x40\0\0\x01\0\0\x5A")
        self.assert_bytes(".Arrays {{$11,$3322,,$44},{,$6655},{,1,0},on,$5A}\n",
                          expected, declarations)
        self.assert_bytes(".Arrays {{},{},{},off,$5A}\n", bytes(28) + b"\x5A", declarations)

    def test_initialized_user_type_arrays_in_unions(self):
        declarations = """.enum Mode
off
on
.ende
.union Arrays
layouts .Layout[2]
choices .Choice[2]
flags .Flags[3]
modes .Mode[3]
larger .byte[16]
.ends
"""
        for initializer, prefix in (("{{$11}}", b"\x11"),
                                    ("{,{,$3322}}", b"\x22\x33"),
                                    ("{,,{,1,0}}", b"\x40"),
                                    ("{,,,on}", b"\x01"),
                                    ("{{}}", b"")):
            with self.subTest(initializer=initializer):
                self.assert_bytes(f".Arrays {initializer}\n.byte $5A\n",
                                  prefix + bytes(16 - len(prefix)) + b"\x5A", declarations)

    def test_invalid_user_type_array_sizes_have_field_diagnostics(self):
        types = """.struc Empty
.ends
.struc Point
px .byte
py .byte
.ends
.enum Mode
off
on
.ende
"""
        for aggregate in ("struc", "union"):
            for field_type, initializer, wrong, diagnostic in (
                    ("Empty", "{}", "1", b"structure initializer expected"),
                    ("Point", "{1,2}", "1", b"structure initializer expected"),
                    ("Choice", "{1}", "1", b"union initializer expected"),
                    ("Flags", "{,1,0}", "1", b"record initializer expected"),
                    ("Mode", "on", "{1}", b"identifier expected")):
                for count in ("0", "-1", "1-1"):
                    declarations = TYPES + types
                    declarations += f".{aggregate} Array\nitems .{field_type}[{count}]\ntail .byte\n.ends\n"
                    for value, expected in ((initializer, b"initializer for field `items' exceeds field size"),
                                            (wrong, diagnostic)):
                        for pure_binary in (False, True):
                            with self.subTest(aggregate=aggregate, field_type=field_type, count=count,
                                              value=value, pure_binary=pure_binary):
                                source = declarations + f".Array {{{value}}}\n"
                                result, _ = self.assemble(source, pure_binary=pure_binary)
                                self.assertEqual(result.returncode, 1, result.stderr.decode())
                                self.assertIn(expected, result.stderr)
                                self.assertEqual(result.stderr.count(b"error:"), 1, result.stderr.decode())
                                self.assertNotIn(b"operand out of range", result.stderr)

    def test_named_array_sizes_have_field_diagnostics(self):
        types = TYPES + ".struc Empty\n.ends\n.struc Point\npx .byte\npy .byte\n.ends\n"
        types += ".enum Mode\noff\non\n.ende\n"
        for field_type, initializer, wrong, diagnostic in (
                ("Empty", "{}", "1", b"structure initializer expected"),
                ("Point", "{1,2}", "1", b"structure initializer expected"),
                ("Choice", "{1}", "1", b"union initializer expected"),
                ("Flags", "{,1,0}", "1", b"record initializer expected"),
                ("Mode", "on", "{1}", b"identifier expected")):
            declarations = types + f".struc Array\nitems .{field_type}[Count]\ntail .byte[4]\n.ends\n"
            for form in ("Count .equ {value}\n", ".define Count {value}\n", "Count = {value}\n"):
                for count in ("0", "-1"):
                    definition = form.format(value=count)
                    for position in ("before_type", "before_instance", "after_instance"):
                        for value, expected in ((initializer, b"initializer for field `items' exceeds field size"),
                                                (wrong, diagnostic)):
                            statement = f".Array {{{value},}}\n"
                            source = {"before_type": definition + declarations + statement,
                                      "before_instance": declarations + definition + statement,
                                      "after_instance": declarations + statement + definition}[position]
                            line = source.splitlines().index(statement.strip()) + 1
                            for pure_binary in (False, True):
                                with self.subTest(field_type=field_type, definition=definition, position=position,
                                                  value=value, pure_binary=pure_binary):
                                    result, _ = self.assemble(source, pure_binary=pure_binary)
                                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                                    self.assertIn(f":{line}: error: ".encode() + expected, result.stderr)
                                    self.assertEqual(result.stderr.count(b"error:"), 1, result.stderr.decode())
                                    self.assertNotIn(b"operand out of range", result.stderr)

    def test_named_array_diagnostics_keep_field_names_in_macros(self):
        source = ".struc Point\npx .byte\npy .byte\n.ends\n"
        source += ".struc First\npoints .Point[Count]\n.ends\n"
        source += ".struc Second\nother_points .Point[Count]\n.ends\n"
        source += ".macro Emit\n.First {{1,2}}\n.Second {{3,4}}\n.endm\nEmit\nEmit\nCount = 0\n"
        for pure_binary in (False, True):
            with self.subTest(pure_binary=pure_binary):
                result, _ = self.assemble(source, pure_binary=pure_binary)
                self.assertEqual(result.returncode, 1, result.stderr.decode())
                for name in ("points", "other_points"):
                    diagnostic = f"initializer for field `{name}' exceeds field size".encode()
                    self.assertEqual(result.stderr.count(diagnostic), 2, result.stderr.decode())
                self.assertEqual(result.stderr.count(b"error:"), 4, result.stderr.decode())

    def test_invalid_array_initializers_do_not_skip_later_errors(self):
        declarations = ".struc Empty\n.ends\n.struc Point\npx .byte\npy .byte\n.ends\n"
        for field_type, initializer, diagnostic in (
                ("Point", "1", b"structure initializer expected"),
                ("Empty", "{}", b"initializer for field `items' exceeds field size")):
            source = declarations + f".struc Array\nitems .{field_type}[0]\n.ends\n"
            source += f".Array {{{initializer}}}\n" * 2
            for pure_binary in (False, True):
                with self.subTest(field_type=field_type, pure_binary=pure_binary):
                    result, _ = self.assemble(source, pure_binary=pure_binary)
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    self.assertEqual(result.stderr.count(diagnostic), 2, result.stderr.decode())
                    self.assertEqual(result.stderr.count(b"error:"), 2, result.stderr.decode())

    def test_nested_user_type_arrays_and_symbolic_counts(self):
        for count, definition in (("1", ""), ("2", ""), ("Count", "Count = 3\n")):
            declarations = f""".struc Point
px .byte
py .byte
.ends
.struc Row
points .Point[{count}]
tail .byte
.ends
.struc Grid
rows .Row[2]
tail .byte
.ends
"""
            size = 2 * (3 if definition else int(count)) + 1
            expected = b"\x01\x02" + bytes(size - 3) + b"\x5A" + bytes(size) + b"\x6B"
            with self.subTest(count=count):
                # A trailing definition exercises unresolved structure sizes too.
                self.assert_bytes(".Grid {{{1,2},$5A},$6B}\n" + definition,
                                  expected, declarations)

    def test_character_omission_positions(self):
        declarations = """.struc Text
lead .char[2]
marker .byte
text .char[6]
tail .char
.ends
"""
        for present in range(16):
            with self.subTest(present=present):
                values = ('"L"', "$11", '"hello"', '"T"')
                parts = (b"L\0", b"\x11", b"hello\0", b"T")
                fields = [value if present & (1 << i) else "" for i, value in enumerate(values)]
                expected = b"".join(part if present & (1 << i) else bytes(len(part))
                                    for i, part in enumerate(parts))
                self.assert_bytes(".Text {" + ",".join(fields) + "}\n", expected, declarations)

    def test_deferred_padding_merges_with_adjacent_storage(self):
        declarations = ".struc Point\npx .byte\npy .byte\n.ends\n"
        declarations += ".struc Array\npoints .Point[Count]\ntail .byte\n.ends\n"
        cases = (("{{1,},$5A}", 1, 0x5A), ("{{},$5A}", 0, 0x5A), ("{{1,},}", 1, 0))
        for case, (initializer, first, tail) in enumerate(cases):
            for count in (1, 2):
                for debug in (False, True):
                    for pure_binary in (False, True):
                        formats = [("text", 1), ("json", 1), ("ndjson", 1)]
                        if pure_binary:
                            formats += [("json", 2), ("ndjson", 2)]
                        for fmt, version in formats:
                            with self.subTest(initializer=initializer, count=count, debug=debug,
                                              pure_binary=pure_binary, fmt=fmt, version=version):
                                results = []
                                for forward in (False, True):
                                    # Keep instance lines and the input path identical for exact debug/object
                                    # and listing comparisons; only the definition's position changes.
                                    source = "\n" if forward else f"Count = {count}\n"
                                    source += declarations + f".Array {initializer}\nAfter: .word After\n"
                                    source += f"Count = {count}\n" if forward else "\n"
                                    listing = self.root / f"merge-{case}-{count}-{debug}-{pure_binary}-{fmt}-{version}-{forward}"
                                    options = [f"--listing={listing}", f"--listing-format={fmt}",
                                               f"--listing-version={version}"]
                                    if debug:
                                        options.append("--debug")
                                    result, binary = self.assemble(source, pure_binary=pure_binary, options=options)
                                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                                    results.append((binary.read_bytes(), listing.read_bytes()))
                                self.assertEqual(results[0], results[1])
                                if not pure_binary:
                                    binary = self.link(binary)
                                expected = bytes([first]) + bytes(2 * count - 1) + bytes([tail])
                                self.assertEqual(binary.read_bytes(), expected + len(expected).to_bytes(2, "little"))

    def test_nested_inner_and_outer_forward_counts(self):
        declarations = ".struc Point\npx .byte\npy .byte\n.ends\n"
        declarations += ".struc Array\npoints .Point[Inner]\ntail .byte\n.ends\n"
        declarations += ".struc Outer\narrays .Array[OuterCount]\nlast .byte\n.ends\n"
        for inner in (1, 2):
            for outer in (1, 2):
                with self.subTest(inner=inner, outer=outer):
                    element = b"\x01\x02" + bytes(2 * (inner - 1)) + b"\x5A"
                    expected = element + bytes(len(element) * (outer - 1)) + b"\x22"
                    # The first expansion sees forward counts; the second sees the assigned values.
                    self.assert_bytes(".Outer {{{1,2},$5A},$22}\n"
                                      + f"Inner = {inner}\nOuterCount = {outer}\n", expected, declarations)
        for inner, outer in ((0, 2), (2, 0)):
            for pure_binary in (False, True):
                with self.subTest(inner=inner, outer=outer, pure_binary=pure_binary):
                    source = declarations + ".Outer {{{1,2},$5A},$22}\n"
                    source += f"Inner = {inner}\nOuterCount = {outer}\n"
                    result, _ = self.assemble(source, pure_binary=pure_binary)
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    self.assertEqual(result.stderr.count(b"error:"), 1, result.stderr.decode())

    def test_unary_forward_array_counts(self):
        declarations = ".struc Point\npx .byte\npy .byte\n.ends\n"
        declarations += ".struc Array\npoints .Point[Count]\ntail .byte\n.ends\n"
        for expression, value, count in (("-Value", -2, 2), ("~Value", -3, 2),
                                          ("<Value", 0x102, 2), ("!Value", 0, 1),
                                          ("-Value", 0, 0)):
            for macro in (False, True):
                for pure_binary in (False, True):
                    with self.subTest(expression=expression, value=value, macro=macro, pure_binary=pure_binary):
                        body = ".Array {{1,2},$5A}\n"
                        if macro:
                            body = ".macro Emit\n" + body + ".endm\nEmit\nEmit\n"
                        source = declarations + body + f"Count .equ {expression}\nValue = {value}\n"
                        result, binary = self.assemble(source, pure_binary=pure_binary)
                        if count == 0:
                            self.assertEqual(result.returncode, 1, result.stderr.decode())
                            self.assertEqual(result.stderr.count(b"error:"), 2 if macro else 1,
                                             result.stderr.decode())
                        else:
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            if not pure_binary:
                                binary = self.link(binary)
                            expected = b"\x01\x02" + bytes(2 * (count - 1)) + b"\x5A"
                            self.assertEqual(binary.read_bytes(), expected * (2 if macro else 1))

    def test_deferred_invalid_user_type_array_counts(self):
        types = TYPES + ".struc Empty\n.ends\n.struc Point\npx .byte\npy .byte\n.ends\n"
        types += ".enum Mode\noff\non\n.ende\n"
        for field_type, initializer in (("Empty", "{}"), ("Point", "{1,2}"),
                                         ("Choice", "{1}"), ("Flags", "{,1,0}"), ("Mode", "on")):
            for definition in ("Count = 0", "Count .equ -1", ".define Count 2-2",
                               "Count .equ Alias\nAlias .equ Final\nFinal = 0"):
                for pure_binary in (False, True):
                    with self.subTest(field_type=field_type, definition=definition,
                                      pure_binary=pure_binary):
                        declarations = types + f".struc Array\nitems .{field_type}[Count]\ntail .byte[4]\n.ends\n"
                        # Positive trailing padding must not hide a negative remainder.
                        source = declarations + f".Array {{{initializer},}}\n" + definition + "\n"
                        output = self.root / ("output.bin" if pure_binary else "output.o")
                        output.write_bytes(b"previous output")
                        result, binary = self.assemble(source, pure_binary=pure_binary)
                        self.assertEqual(result.returncode, 1, result.stderr.decode())
                        self.assertEqual(result.stderr.count(b"error:"), 1, result.stderr.decode())
                        self.assertEqual(binary.read_bytes(), b"previous output")

    def test_deferred_errors_in_repeated_initializers(self):
        declarations = ".struc Point\npx .byte\npy .byte\n.ends\n"
        declarations += ".struc Array\nitems .Point[Count]\n.ends\n"
        instance = ".Array {{1,2}}\n"
        for body in (instance * 2, ".macro Emit\n" + instance + ".endm\nEmit\nEmit\n",
                     ".rept 2\n" + instance + ".endm\n"):
            for pure_binary in (False, True):
                with self.subTest(body=body, pure_binary=pure_binary):
                    result, _ = self.assemble(declarations + body + "Count = 0\n",
                                              pure_binary=pure_binary)
                    self.assertEqual(result.returncode, 1, result.stderr.decode())
                    self.assertEqual(result.stderr.count(b"error:"), 2, result.stderr.decode())

    def test_deferred_valid_counts_and_following_addresses(self):
        types = TYPES + ".struc Empty\n.ends\n.struc Point\npx .byte\npy .byte\n.ends\n"
        types += ".enum Mode\noff\non\n.ende\n"
        for case, (field_type, initializer, element) in enumerate((
                ("Empty", "{}", b""), ("Point", "{1,2}", b"\x01\x02"),
                ("Choice", "{1}", b"\x01\0\0\0"), ("Flags", "{,1,0}", b"\x40"),
                ("Mode", "on", b"\x01"))):
            for count in (1, 2, 3):
                for debug in (False, True):
                    for pure_binary in (False, True):
                        with self.subTest(field_type=field_type, count=count, debug=debug,
                                          pure_binary=pure_binary):
                            source = types + f".struc Array\nitems .{field_type}[Count]\ntail .byte\n.ends\n"
                            source += ".org $8000\n" if pure_binary else ""
                            source += f".Array {{{initializer},$5A}}\nAfter: .word After\n"
                            source += f"Count .equ Alias\nAlias .equ Final\nFinal = {count}\n"
                            expected = element + bytes(len(element) * (count - 1)) + b"\x5A"
                            address = 0x8000 + len(expected)
                            options = ["--debug"] if debug else []
                            if pure_binary:
                                listing = self.root / f"forward-{case}-{count}-{debug}.json"
                                options += [f"--listing={listing}", "--listing-format=json"]
                            result, binary = self.assemble(source, pure_binary=pure_binary, options=options)
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            if pure_binary:
                                rows = json.loads(listing.read_text())["records"]
                                after = next(row for row in rows if row["directive_or_opcode"] == "After")
                                self.assertEqual(after["output_offset_start"], len(expected))
                                self.assertEqual(after["cpu_address_start"], f"0x{address:04X}")
                            else:
                                binary = self.link(binary, origin=0x8000)
                            self.assertEqual(binary.read_bytes(), expected + address.to_bytes(2, "little"))

    def test_array_counts_keep_instance_time_assignments(self):
        declarations = ".struc Point\npx .byte\npy .byte\n.ends\n"
        declarations += ".struc Array\nitems .Point[Count]\ntail .byte\n.ends\n"
        for pure_binary in (False, True):
            with self.subTest(pure_binary=pure_binary):
                source = declarations + "Count = 1\n.Array {{1,2},$5A}\nCount = 2\n.Array {{3,4},$6B}\nCount = 0\n"
                result, binary = self.assemble(source, pure_binary=pure_binary)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                if not pure_binary:
                    binary = self.link(binary)
                self.assertEqual(binary.read_bytes(), bytes.fromhex("01 02 5A 03 04 00 00 6B"))
                result, _ = self.assemble(declarations + "Count = 0\n.Array {{1,2},$5A}\nCount = 2\n",
                                          pure_binary=pure_binary)
                self.assertEqual(result.returncode, 1, result.stderr.decode())
                self.assertEqual(result.stderr.count(b"error:"), 1, result.stderr.decode())

    def test_sizeof_data_with_deferred_array_padding(self):
        declarations = ".struc Point\npx .byte\npy .byte\n.ends\n"
        declarations += ".struc Array\nitems .Point[Count]\ntail .byte\n.ends\n"
        source = declarations + "Payload: .Array {{1,2},$5A}\nCount = 2\nEnd:\n.byte sizeof(Payload)\n"
        for pure_binary in (False, True):
            with self.subTest(pure_binary=pure_binary):
                result, binary = self.assemble(source, pure_binary=pure_binary)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                if not pure_binary:
                    binary = self.link(binary)
                self.assertEqual(binary.read_bytes(), bytes.fromhex("01 02 00 00 5A 05"))

    def test_circular_array_counts_fail_cleanly(self):
        declarations = ".struc Point\npx .byte\npy .byte\n.ends\n"
        declarations += ".struc Array\nitems .Point[Count]\n.ends\n"
        source = declarations + ".Array {{1,2}}\nCount .equ Alias\nAlias .equ Count\n"
        for pure_binary in (False, True):
            with self.subTest(pure_binary=pure_binary):
                result, _ = self.assemble(source, pure_binary=pure_binary)
                self.assertEqual(result.returncode, 1, result.stderr.decode())
                self.assertEqual(result.stderr.count(b"circular definition in field size"), 1,
                                 result.stderr.decode())

    def test_character_storage_and_short_strings(self):
        declarations = """.struc Sample
number .byte
gap .word
text .char[6]
.ends
.union TextChoice
text .char[6]
wide .word
.ends
"""
        cases = (
            (".char[4]\n.byte $5A\n", bytes(4) + b"\x5A"),
            ('.Sample {10,,"hello"}\n', b"\x0A\0\0hello\0"),
            ('.Sample {,,"ab"}\n', bytes(3) + b"ab" + bytes(4)),
            ('.TextChoice {"ab",}\n', b"ab" + bytes(4)),
            ('.TextChoice {,$3322}\n', b"\x22\x33" + bytes(4)),
            ('.TextChoice {,}\n', bytes(6)),
        )
        for body, expected in cases:
            with self.subTest(body=body):
                self.assert_bytes(body, expected, declarations)
        charmap = self.root / "charmap.txt"
        charmap.write_text("a-b=0xC0\n")
        self.assert_bytes(f'.charmap "{charmap}"\n.Sample {{,,"ab"}}\n',
                          bytes(3) + b"\xC0\xC1" + bytes(4), declarations)

    def test_addresses_after_initialized_or_omitted_fields(self):
        declarations = """.struc Text
text .char[6]
tail .byte
.ends
.struc Inline
head .byte
.union
small .byte
wide .word
.ends
tail .byte
.ends
.struc Point
px .byte
py .byte
.ends
.struc Array
points .Point[2]
tail .byte
.ends
.union ArrayChoice
points .Point[2]
wide .byte[6]
.ends
"""
        cases = (
            (".Array {{1,2},$5A}\n", bytes.fromhex("01 02 00 00 5A")),
            (".Array {{1,2},}\n", bytes.fromhex("01 02 00 00 00")),
            (".ArrayChoice {{1,2}}\n", bytes.fromhex("01 02 00 00 00 00")),
            (".Text {,$5A}\n", bytes(6) + b"\x5A"),
            (".Layout {,$3322,,}\n", bytes.fromhex("00 22 33 00 00 00 00")),
            (".Nested {,,,,$9988}\n", bytes(13) + b"\x88\x99"),
            (".Inline {$11,}\n", b"\x11\0\0\0"),
            (".char[4]\n", bytes(4)),
        )
        for case_index, (body, expected) in enumerate(cases):
            for debug in (False, True):
                for pure_binary in (False, True):
                    with self.subTest(body=body, debug=debug, pure_binary=pure_binary):
                        source = TYPES + declarations + (".org $8000\n" if pure_binary else "")
                        source += body + "After: .word After\n"
                        options = ["--debug"] if debug else []
                        if pure_binary:
                            # Earlier subtests must not supply missing output artifacts.
                            listing = self.root / f"listing-{case_index}-{int(debug)}.json"
                            xref = self.root / f"xref-{case_index}-{int(debug)}.json"
                            options += [f"--listing={listing}", "--listing-format=json", f"--xref={xref}"]
                        result, binary = self.assemble(source, pure_binary=pure_binary, options=options)
                        self.assertEqual(result.returncode, 0, result.stderr.decode())
                        address = 0x8000 + len(expected)
                        if pure_binary:
                            rows = json.loads(listing.read_text())["records"]
                            after = next(row for row in rows if row["directive_or_opcode"] == "After")
                            self.assertEqual(after["output_offset_start"], len(expected))
                            self.assertEqual(after["cpu_address_start"], f"0x{address:04X}")
                            symbols = json.loads(xref.read_text())["symbols"]
                            symbol = next(symbol for symbol in symbols if symbol["name"] == "After")
                            self.assertEqual(symbol["definition"]["cpu_address"], f"0x{address:04X}")
                        else:
                            binary = self.link(binary, origin=0x8000)
                        self.assertEqual(binary.read_bytes(), expected + address.to_bytes(2, "little"))

    def test_builtin_array_gaps_have_specific_diagnostic(self):
        declarations = ".union Buffer\nvalues .byte[3]\n.ends\n"
        for initializer in ("{1,,2}", "{,2}", "{,}"):
            for body in (f".Layout {{$11,$2233,{initializer},$44}}\n",
                         f".Buffer {{{initializer}}}\n", f".byte {initializer}\n"):
                for pure_binary in (False, True):
                    with self.subTest(body=body, pure_binary=pure_binary):
                        result, _ = self.assemble(TYPES + declarations + body, pure_binary=pure_binary)
                        self.assertEqual(result.returncode, 1, result.stderr.decode())
                        self.assertIn(b"omitted elements are not supported in built-in array initializers",
                                      result.stderr)
                        self.assertEqual(result.stderr.count(b"error:"), 1, result.stderr.decode())

    def test_macro_parameters(self):
        result, binary = self.assemble(TYPES + """.macro Emit index_value
.Flags {,1,index_value}
.Layout {,index_value,,}
.Choice {,,,{,1,index_value}}
.endm
Emit 5
Emit 9
""")
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(binary.read_bytes(), bytes.fromhex(
            "45 00 05 00 00 00 00 00 45 00 00 00"
            "49 00 09 00 00 00 00 00 49 00 00 00"))

    def test_excess_omissions_still_count_as_fields(self):
        for body in (".Flags {,,,}\n", ".Layout {,,,,}\n", ".Choice {,,,,}\n"):
            with self.subTest(body=body):
                result, _ = self.assemble(TYPES + body)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"too many field initializers", result.stderr)

    def test_union_rejects_multiple_explicit_fields(self):
        result, _ = self.assemble(TYPES + ".Choice {$11,,$77665544,}\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"only one field of union can be initialized", result.stderr)


if __name__ == "__main__":
    unittest.main()
