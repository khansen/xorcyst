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

    def assemble(self, source, *, pure_binary=True, options=()):
        assembly = self.root / "input.asm"
        binary = self.root / ("output.bin" if pure_binary else "output.o")
        assembly.write_text(source)
        result = subprocess.run([str(XASM), *(["--pure-binary"] if pure_binary else []),
                                 *options, str(assembly), "-o", str(binary)],
                                capture_output=True, timeout=10)
        return result, binary

    def link(self, object_file, origin=0):
        script, binary = self.root / "link.script", self.root / "linked.bin"
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

    def test_reported_record_reproduction(self):
        self.assert_bytes(".Flags {, 1, 0}\n", b"\x40")

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
