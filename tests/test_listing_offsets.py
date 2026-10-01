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
