#!/usr/bin/env python3
"""Emitting rows keep binary offsets across storage and segment switches."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


XASM = Path(sys.argv.pop(1)).resolve()


class ListingOffsets(unittest.TestCase):
    def test_version2_emission_spans_and_instruction_joins(self):
        source_text = """.ORG $8000
LDA #$42
.DSW 3
.DATASEG
.ORG $8000
Ram:
.DB 1,2,3,4,5,6
.INCBIN "payload.bin"
.DSD 2
.CODESEG
.macro Emit
.DB $A5
NOP
.endm
Emit
Emit
.ORG $8000
.INCBIN "payload.bin"
.rept 2
.DSB 2
.endm
Tail: RTS
.DATASEG
RamEnd:
.CODESEG
"""
        expected = bytes.fromhex("A9 42") + bytes(6) + bytes.fromhex("A5 EA A5 EA 91 92 93 94 95 96")
        expected += bytes(4) + b"\x60"
        for debug in (False, True):
            with self.subTest(debug=debug), tempfile.TemporaryDirectory(prefix="xasm-listing-v2-") as directory:
                root = Path(directory)
                source, binary, listing, xref = (root / name for name in ("input.asm", "out.bin", "listing", "xref"))
                source.write_text(source_text)
                (root / "payload.bin").write_bytes(bytes.fromhex("91 92 93 94 95 96"))
                command = [str(XASM), "--pure-binary", str(source), "-o", str(binary)]
                if debug:
                    command.append("--debug")
                plain = subprocess.run(command, cwd=root, capture_output=True)
                self.assertEqual(plain.returncode, 0, plain.stderr.decode())
                self.assertEqual(binary.read_bytes(), expected)
                formats = []
                for format_name in ("json", "ndjson"):
                    result = subprocess.run(command + [f"--listing={listing}", f"--listing-format={format_name}",
                                                        "--listing-version=2", f"--xref={xref}",
                                                        "--xref-instructions=true"],
                                            cwd=root, capture_output=True)
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    self.assertEqual(result.stderr, plain.stderr)
                    self.assertEqual(binary.read_bytes(), expected)
                    if format_name == "json":
                        document = json.loads(listing.read_text())
                        self.assertEqual(document["version"], "2")
                        self.assertTrue(document["binary_generated"])
                        rows = document["records"]
                    else:
                        rows = [json.loads(line) for line in listing.read_text().splitlines()]
                    formats.append(rows)
                    emitted, data_size = bytearray(), 0
                    by_offset = {}
                    for row in rows:
                        self.assertEqual(row["version"], "2")
                        self.assertTrue(row["binary_generated"])
                        count = row["byte_count"]
                        cpu = int(row["cpu_address_start"], 16)
                        self.assertEqual(row["cpu_address_end"], f"0x{(cpu + max(count - 1, 0)) & 0xFFFF:04X}")
                        if row["emits_bytes"]:
                            self.assertEqual(row["segment"], "code")
                            self.assertGreater(count, 0)
                            start = row["output_offset_start"]
                            self.assertEqual(start, len(emitted))
                            self.assertEqual(row["output_offset_end"], start + count - 1)
                            if row["fill_byte"] is not None:
                                self.assertEqual(row["fill_byte"], "00")
                                self.assertEqual(row["bytes_hex"], [])
                                values = bytes(count)
                            else:
                                values = bytes.fromhex("".join(row["bytes_hex"]))
                                self.assertEqual(len(values), count)
                            self.assertEqual(expected[start:start + count], values)
                            emitted.extend(values)
                            by_offset[start] = row
                        else:
                            self.assertIsNone(row["output_offset_start"])
                            self.assertIsNone(row["output_offset_end"])
                            self.assertIsNone(row["fill_byte"])
                            self.assertEqual(row["bytes_hex"], [])
                            if row["segment"] == "data":
                                data_size += count
                            else:
                                self.assertEqual(count, 0)
                    self.assertEqual(emitted, expected)
                    self.assertEqual(data_size, 20)
                    labels = {row["directive_or_opcode"]: row for row in rows}
                    self.assertEqual(labels["Ram"]["segment"], "data")
                    self.assertEqual(labels["Ram"]["cpu_address_start"], "0x8000")
                    self.assertEqual(labels["RamEnd"]["cpu_address_start"], "0x8014")
                    self.assertEqual(labels["Tail"]["cpu_address_start"], "0x800A")
                    records = json.loads(xref.read_text())["instruction_records"]["records"]
                    self.assertEqual(len(records), 4)
                    for record in records:
                        row = by_offset[record["output_offset"]]
                        self.assertEqual(row["bytes_hex"], [f"{value:02X}" for value in record["bytes"]])
                self.assertEqual(formats[0], formats[1])

    def test_version2_non_emitting_and_empty_programs(self):
        for source_text in ("\n", ".DATASEG\n.DB 1,2,3,4,5\n.DSB 3\n"):
            for format_name in ("json", "ndjson"):
                with self.subTest(source=source_text, format=format_name), tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    source, binary, listing = (root / name for name in ("input.asm", "out.bin", "listing"))
                    source.write_text(source_text)
                    result = subprocess.run([str(XASM), "--pure-binary", str(source), "-o", str(binary),
                                             f"--listing={listing}", f"--listing-format={format_name}",
                                             "--listing-version=2"], capture_output=True)
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    self.assertEqual(binary.read_bytes(), b"")
                    rows = (json.loads(listing.read_text())["records"] if format_name == "json"
                            else [json.loads(line) for line in listing.read_text().splitlines()])
                    for row in rows:
                        self.assertTrue(row["binary_generated"])
                        self.assertFalse(row["emits_bytes"])
                        self.assertIsNone(row["output_offset_start"])
                        self.assertEqual(row["bytes_hex"], [])

    def test_version2_diagnostic_listing_does_not_claim_output(self):
        for format_name in ("json", "ndjson"):
            with self.subTest(format=format_name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source, binary, listing = (root / name for name in ("input.asm", "out.bin", "listing"))
                source.write_text(".DB Missing\n.DSB 4\nNOP\n")
                binary.write_bytes(b"previous output")
                result = subprocess.run([str(XASM), "--pure-binary", str(source), "-o", str(binary),
                                         f"--listing={listing}", f"--listing-format={format_name}",
                                         "--listing-version=2"], capture_output=True)
                self.assertEqual(result.returncode, 1, result.stderr.decode())
                self.assertEqual(binary.read_bytes(), b"previous output")
                rows = (json.loads(listing.read_text())["records"] if format_name == "json"
                        else [json.loads(line) for line in listing.read_text().splitlines()])
                self.assertTrue(rows)
                for row in rows:
                    self.assertFalse(row["binary_generated"])
                    self.assertFalse(row["emits_bytes"])
                    self.assertIsNone(row["output_offset_start"])
                    self.assertIsNone(row["output_offset_end"])
                    self.assertIsNone(row["fill_byte"])
                    self.assertEqual(row["bytes_hex"], [])

    def test_version2_cli_requirements(self):
        cases = (("--listing-version=0",), ("--listing-version=3",), ("--listing-version=2x",),
                 ("--listing-version=2",),
                 ("--listing-version=2", "--listing=listing", "--pure-binary"),
                 ("--listing-version=2", "--listing=listing", "--listing-format=json"))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "input.asm").write_text("NOP\n")
            for arguments in cases:
                with self.subTest(arguments=arguments):
                    (root / "listing").write_bytes(b"previous listing")
                    (root / "out.bin").write_bytes(b"previous binary")
                    result = subprocess.run([str(XASM), "input.asm", "-o", "out.bin", *arguments],
                                            cwd=root, capture_output=True)
                    self.assertEqual(result.returncode, 2, result.stderr.decode())
                    self.assertIn(b"--listing-version", result.stderr)
                    self.assertEqual((root / "listing").read_bytes(), b"previous listing")
                    self.assertEqual((root / "out.bin").read_bytes(), b"previous binary")

    def test_explicit_version1_matches_default(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "input.asm").write_text("NOP\n.DATASEG\n.DB 1,2,3,4,5\n.DSB 7\n.CODESEG\nRTS\n")
            for format_name in ("text", "json", "ndjson"):
                with self.subTest(format=format_name):
                    command = [str(XASM), "--pure-binary", "input.asm", "-o", "out.bin",
                               "--listing=listing", f"--listing-format={format_name}"]
                    result = subprocess.run(command, cwd=root, capture_output=True)
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    default = (root / "listing").read_bytes()
                    result = subprocess.run(command + ["--listing-version=1"], cwd=root, capture_output=True)
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    self.assertEqual((root / "listing").read_bytes(), default)

    def test_storage_followed_by_data_and_instructions(self):
        for debug in (False, True):
            for directive, size in ((".DSB 7", 7), (".DSW 3", 6), (".DSD 2", 8)):
                for format_name in ("json", "ndjson"):
                    with self.subTest(debug=debug, storage=directive, format=format_name):
                        with tempfile.TemporaryDirectory(prefix="xasm-listing-offsets-") as directory:
                            root = Path(directory)
                            source, binary, listing = (root / name for name in ("input.asm", "out.bin", "listing"))
                            source.write_text(f""".ORG $8000
LDA #$42
{directive}
.DATASEG
.ORG $20
.DSW 5
.CODESEG
Table: .DB $34,$92,$78,$96,$BC,$9A
LDA Table
.ORG $C000
.DSB 2
RTS
""")
                            command = [str(XASM), "--pure-binary", "-o", str(binary), str(source)]
                            if debug:
                                command.append("--debug")
                            plain = subprocess.run(command, capture_output=True)
                            self.assertEqual(plain.returncode, 0, plain.stderr.decode())
                            expected = bytes([0xA9, 0x42]) + bytes(size) + bytes.fromhex("34 92 78 96 BC 9A")
                            expected += bytes([0xAD, size + 2, 0x80, 0, 0, 0x60])
                            self.assertEqual(binary.read_bytes(), expected)
                            result = subprocess.run(command + [f"--listing={listing}", f"--listing-format={format_name}"],
                                                    capture_output=True)
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            self.assertEqual(result.stderr, plain.stderr)
                            self.assertEqual(binary.read_bytes(), expected)
                            rows = (json.loads(listing.read_text())["records"] if format_name == "json"
                                    else [json.loads(line) for line in listing.read_text().splitlines()])
                            storage = [row for row in rows if row["line"] == 3
                                       and row["directive_or_opcode"] == ".DSB"]
                            self.assertEqual(len(storage), 1)
                            self.assertEqual(storage[0]["bytes_hex"], [])
                            self.assertEqual(storage[0]["output_offset_start"], 2)
                            table = next(row for row in rows if row["directive_or_opcode"] == "Table")
                            self.assertEqual(table["output_offset_start"], size + 2)
                            self.assertEqual(table["cpu_address_start"], f"0x{0x8002 + size:04X}")
                            data = [row for row in rows if row["directive_or_opcode"] == ".DB"]
                            self.assertEqual([row["output_offset_start"] for row in data], [size + 2, size + 6])
                            instructions = [row for row in rows if row["directive_or_opcode"] in ("LDA", "RTS")]
                            self.assertEqual([row["output_offset_start"] for row in instructions], [0, size + 8, size + 13])
                            self.assertEqual(instructions[-1]["cpu_address_start"], "0xC002")
                            for row in rows:
                                values = bytes(int(value, 16) for value in row["bytes_hex"])
                                if values:
                                    start = row["output_offset_start"]
                                    self.assertEqual(expected[start:start + len(values)], values)
                                    self.assertEqual(row["output_offset_end"], start + len(values) - 1)

    def test_non_emitting_data_does_not_shift_later_offsets(self):
        cases = ((".DB 1,2,3", bytes([1, 2, 3])),
                 (".DB 1,2,3,4,5,6", bytes(range(1, 7))),
                 ('.INCBIN "payload.bin"', bytes([0x91, 0x92])),
                 ('.INCBIN "payload.bin"', bytes(range(0x91, 0x97))))
        for debug in (False, True):
            for directive, payload in cases:
                for format_name in ("json", "ndjson"):
                    with self.subTest(debug=debug, data=directive, size=len(payload), format=format_name):
                        with tempfile.TemporaryDirectory(prefix="xasm-listing-segments-") as directory:
                            root = Path(directory)
                            (root / "payload.bin").write_bytes(payload)
                            source, binary, listing = (root / name for name in ("input.asm", "out.bin", "listing"))
                            source.write_text(f""".ORG $8000
LDA #$42
.DATASEG
.ORG $0300
Buf:
{directive}
BufferEnd:
.CODESEG
Next:
RTS
.DSB 2
Tail: .DB $A5,$5A
.DATASEG
Gap: .DSW 2
.CODESEG
Last: NOP
""")
                            command = [str(XASM), "--pure-binary", "-o", str(binary), str(source)]
                            if debug:
                                command.append("--debug")
                            plain = subprocess.run(command, cwd=root, capture_output=True)
                            self.assertEqual(plain.returncode, 0, plain.stderr.decode())
                            expected = bytes.fromhex("A9 42 60 00 00 A5 5A EA")
                            self.assertEqual(binary.read_bytes(), expected)
                            result = subprocess.run(command + [f"--listing={listing}", f"--listing-format={format_name}"],
                                                    cwd=root, capture_output=True)
                            self.assertEqual(result.returncode, 0, result.stderr.decode())
                            self.assertEqual(result.stderr, plain.stderr)
                            self.assertEqual(binary.read_bytes(), expected)
                            rows = (json.loads(listing.read_text())["records"] if format_name == "json"
                                    else [json.loads(line) for line in listing.read_text().splitlines()])
                            labels = {row["directive_or_opcode"]: row for row in rows if not row["bytes_hex"]}
                            for name, offset, cpu in (("Buf", 2, 0x0300),
                                                      ("BufferEnd", 2, 0x0300 + len(payload)),
                                                      ("Next", 2, 0x8002), ("Tail", 5, 0x8005),
                                                      ("Gap", 7, 0x0300 + len(payload)),
                                                      ("Last", 7, 0x8007)):
                                self.assertEqual(labels[name]["output_offset_start"], offset, name)
                                self.assertEqual(labels[name]["cpu_address_start"], f"0x{cpu:04X}", name)
                            in_data = False
                            non_emitted = bytearray()
                            for row in rows:
                                if row["directive_or_opcode"] in (".DATASEG", ".CODESEG"):
                                    in_data = row["directive_or_opcode"] == ".DATASEG"
                                values = bytes.fromhex("".join(row["bytes_hex"]))
                                if in_data:
                                    non_emitted.extend(values)
                                elif values:
                                    start = row["output_offset_start"]
                                    self.assertEqual(expected[start:start + len(values)], values)
                                    self.assertEqual(row["output_offset_end"], start + len(values) - 1)
                            self.assertEqual(non_emitted, payload)


if __name__ == "__main__":
    unittest.main()
