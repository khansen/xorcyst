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
