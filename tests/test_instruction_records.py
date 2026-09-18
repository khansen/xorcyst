#!/usr/bin/env python3
"""Producer contract tests; fixtures contain no project-specific inputs."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


XASM = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "xasm"
XLNK = Path(os.environ.get("XLNK", Path(__file__).resolve().parents[1] / "xlnk")).resolve()


class InstructionRecords(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="xasm-instructions-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def assemble(self, text, *, options=(), includes=None):
        source = self.root / "input.asm"
        source.write_bytes(text if isinstance(text, bytes) else text.encode())
        for name, content in (includes or {}).items():
            (self.root / name).write_text(content)
        output = self.root / "output.bin"
        xref = self.root / "xref.json"
        base = [str(XASM), "--pure-binary", *options, str(source), "-o", str(output)]
        plain = subprocess.run([arg for arg in base if not arg.startswith("--xref-")], capture_output=True)
        self.assertEqual(plain.returncode, 0, plain.stderr.decode())
        plain_bytes = output.read_bytes()
        baseline = subprocess.run([*base, f"--xref={xref}"], capture_output=True)
        self.assertEqual(baseline.returncode, 0, baseline.stderr.decode())
        old_xref = json.loads(xref.read_text())
        result = subprocess.run([*base, f"--xref={xref}", "--xref-instructions=true"], capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(result.stderr, plain.stderr)
        self.assertEqual(output.read_bytes(), plain_bytes)
        data = json.loads(xref.read_text())
        records = data.pop("instruction_records")
        self.assertEqual(records["version"], "1")
        data["build"].pop("timestamp_utc")
        old_xref["build"].pop("timestamp_utc")
        self.assertEqual(data, old_xref, "opt-in must not alter legacy xref sections")
        ids = [record["origin_id"] for record in records["records"]]
        self.assertEqual(ids, sorted(set(ids)))
        for record in records["records"]:
            start = record["output_offset"]
            self.assertEqual(record["size"], len(record["bytes"]))
            self.assertEqual(bytes(record["bytes"]), plain_bytes[start:start + record["size"]])
            self.assertEqual(record["opcode"], record["bytes"][0])
        self.assert_source_spans(records["records"])
        sidecar = self.root / "instructions.json"
        manifest = self.root / "dependencies.json"
        for mode in ("separate", "legacy-and-separate", "both"):
            with self.subTest(packaging=mode):
                args = [arg for arg in base if not arg.startswith("--xref-")] if mode == "separate" else list(base)
                args += [f"--instruction-records-output={sidecar}", f"--dependency-manifest={manifest}"]
                if mode != "separate":
                    args += [f"--xref={xref}"]
                args += ["--xref-instructions=" + ("true" if mode == "both" else "false")]
                run = subprocess.run(args, capture_output=True)
                self.assertEqual(run.returncode, 0, run.stderr.decode())
                self.assertEqual(run.stderr, plain.stderr)
                self.assertEqual(output.read_bytes(), plain_bytes)
                self.assertEqual(json.loads(sidecar.read_bytes()), records)
                self.assertEqual(json.loads(manifest.read_bytes())["invocation"]["argv"], args)
                if mode != "separate":
                    actual = json.loads(xref.read_bytes())
                    if mode == "both":
                        self.assertEqual(actual.pop("instruction_records"), records)
                    else:
                        self.assertNotIn("instruction_records", actual)
                    actual["build"].pop("timestamp_utc")
                    self.assertEqual(actual, old_xref)
        return records["records"]

    def assert_source_spans(self, value):
        if isinstance(value, dict):
            if "span" in value and "text" in value:
                span = value["span"]
                lines = Path(span["file"]).read_bytes().split(b"\n")
                start = sum(len(line) + 1 for line in lines[:span["line"] - 1]) + span["column"] - 1
                end = sum(len(line) + 1 for line in lines[:span["end_line"] - 1]) + span["end_column"] - 1
                raw = Path(span["file"]).read_bytes()
                self.assertEqual(value["text"].encode(), raw[start:end])
            for child in value.values():
                self.assert_source_spans(child)
        elif isinstance(value, list):
            for child in value:
                self.assert_source_spans(child)

    def test_literal_modes_and_emitted_bytes(self):
        records = self.assemble(""".ORG $C000
Start:
    LDA #$0F
    LDA $10
    LDA $0010,X
    LDX $10,Y
    LDA $C123
    LDA $C123,X
    LDA $C123,Y
    LDA [$10,X]
    LDA [$10],Y
    JMP [$C000]
    BNE $+2
    ASL A
    RTS
END
""")
        self.assertEqual([r["addressing_mode"] for r in records], [
            "immediate", "zeropage", "zeropage_x", "zeropage_y", "absolute",
            "absolute_x", "absolute_y", "preindexed_indirect", "postindexed_indirect",
            "indirect", "relative", "accumulator", "implied",
        ])
        self.assertEqual(records[0]["source"]["text"], "LDA #$0F")
        self.assertEqual(records[0]["expression"]["source"]["text"], "$0F")
        self.assertEqual(records[2]["expression"]["source"]["text"], "$0010")
        self.assertEqual(records[10]["expression"]["kind"], "operator")
        self.assertEqual(records[10]["expression"]["operator"], "+")
        self.assertEqual(records[10]["expression"]["children"][0]["kind"], "current_pc")
        self.assertEqual(records[10]["branch_displacement"], 0)
        self.assertEqual(records[10]["operand_source"]["text"], "$+2")
        self.assertEqual(records[7]["operand_source"]["text"], "[$10,X]")
        self.assertEqual(records[0]["operand_source"]["text"], "#$0F")
        self.assertTrue(records[0]["immediate"])
        self.assertFalse(records[1]["immediate"])
        self.assertIsNone(records[12]["expression"])
        self.assertIsNone(records[12]["operand_value"])

    def test_operand_form_classification(self):
        records = self.assemble(""".ORG $C000
Target:
    LDA #$0F
    JSR Target
    LDA Target+1
    RTS
END
""")
        self.assertEqual([r["operand_form"] for r in records],
                          ["integer_literal", "symbol", "expression", "none"])
        self.assertEqual(records[0]["referenced_symbols"], [])
        self.assertEqual(records[1]["referenced_symbols"], ["Target"])
        self.assertEqual(records[2]["referenced_symbols"], ["Target"])
        self.assertEqual(records[3]["referenced_symbols"], [])

    def test_equates_before_folding_and_final_zero_page_mode(self):
        records = self.assemble(""".ORG 0
COUNT .EQU 3
ZP_Scratch .EQU $10
Start:
    LDA #COUNT
    STA ZP_Scratch
    LDA Target
    BNE Target
Target:
    RTS
END
""")
        self.assertEqual(records[0]["referenced_symbols"], ["COUNT"])
        self.assertEqual(records[0]["expression"]["kind"], "symbol")
        self.assertEqual(records[0]["operand_value"], 3)
        self.assertEqual(records[2]["addressing_mode"], "zeropage")
        self.assertEqual(records[2]["parsed_addressing_mode"], "absolute")
        self.assertEqual(records[2]["operand_value"], records[4]["cpu_address"])
        self.assertEqual(records[3]["operand_value"], records[4]["cpu_address"])

    def test_macro_arguments_and_repeated_expansion(self):
        records = self.assemble("""MACRO LOAD value
    LDA #value
ENDM
.ORG $C000
Start:
    LOAD $01
    LOAD $02
    REPT 2
        LDA $10
    ENDM
    RTS
END
""")
        self.assertEqual(len(records), 5)
        self.assertEqual([r["operand_value"] for r in records[:2]], [1, 2])
        self.assertEqual([r["source"]["text"] for r in records[:2]], ["LDA #value"] * 2)
        self.assertEqual([r["use"]["line"] for r in records[:2]], [6, 7])
        self.assertEqual([r["expression"]["source"]["text"] for r in records[:2]], ["$01", "$02"])
        self.assertEqual(records[2]["source"], records[3]["source"])
        self.assertEqual(records[0]["operand_source"]["text"], "#value")

    def test_include_segments_same_line_and_long_lines(self):
        records = self.assemble(".ORG $C000\nFirst:\n\tLDA $10 : LDX $11\n.INCSRC \"part.asm\"\n.ORG $C000\nSecond:\n" + " " * 1300 + "LDA $12\nEND\n",
                                includes={"part.asm": "    LDA $13\n"})
        self.assertEqual(len(records), 4)
        self.assertEqual(records[0]["use"]["line"], records[1]["use"]["line"])
        self.assertNotEqual(records[0]["use"]["column"], records[1]["use"]["column"])
        self.assertEqual(records[0]["source"]["text"], "LDA $10")
        self.assertEqual(records[1]["source"]["text"], "LDX $11")
        self.assertTrue(records[2]["source"]["span"]["file"].endswith("part.asm"))
        self.assertEqual(records[3]["source"]["text"], "LDA $12")
        self.assertEqual(records[3]["source"]["span"]["column"], 1301)
        self.assertEqual(records[0]["cpu_address"], records[3]["cpu_address"])
        self.assertNotEqual(records[0]["segment_id"], records[3]["segment_id"])
        self.assertEqual([r["lexical_owner"] for r in records], ["First", "First", "First", "Second"])

    def test_empty(self):
        self.assertEqual(self.assemble(".ORG $C000\n.DB 1,2,3\nEND\n"), [])

    def test_empty_file_parse_error_cannot_certify_success(self):
        source = self.root / "input.asm"
        source.write_text("")
        xref = self.root / "xref.json"
        run = subprocess.run([str(XASM), "--pure-binary", f"--xref={xref}", "--xref-instructions=true",
                              str(source), "-o", str(self.root / "out.bin")], capture_output=True)
        self.assertNotEqual(run.returncode, 0, run.stderr.decode())
        self.assertFalse(xref.exists())

    def test_debug_and_filters_do_not_change_stream(self):
        text = """.ORG $C000
Start:
@@loop:
    LDA #$01
    BNE @@loop
    BEQ +
+   RTS
END
"""
        expected = self.assemble(text)
        for options in [("--debug",), ("--xref-include-locals=true", "--xref-include-anon=true"),
                        ("--xref-data=true", "--xref-include-owner=true")]:
            with self.subTest(options=options):
                self.assertEqual(self.assemble(text, options=options), expected)

    def test_spelling_grouping_case_and_base_refusals(self):
        records = self.assemble(""".ORG $C000
CONST .EQU 5
Start:
    lda #(CONST + 1)
    lda 16
    lda $0010
    lda %10000
    lda Table-1,Y
    lda Table+CONST,Y
    lda Table+Other,Y
    lda #<Table
Table:
    .DB 0
Other:
    .DB 0
END
""")
        self.assertEqual(records[0]["operand_source"]["text"], "#(CONST + 1)")
        self.assertEqual(records[0]["mnemonic"], "LDA")
        self.assertEqual([r["operand_value"] for r in records[1:4]], [16] * 3)
        self.assertEqual([r["expression"]["source"]["text"] for r in records[1:4]], ["16", "$0010", "%10000"])
        self.assertEqual(records[4]["structural_base"], {"symbol": "Table", "displacement": -1, "projection": "none"})
        self.assertEqual(records[4]["index_register"], "Y")
        self.assertIsNone(records[5]["structural_base"])
        self.assertIsNone(records[6]["structural_base"])
        self.assertEqual(records[7]["structural_base"], {"symbol": "Table", "displacement": 0, "projection": "low"})

    def test_mutable_values_and_inactive_code(self):
        records = self.assemble(""".ORG $C000
VALUE = 1
Start:
    LDA #VALUE
VALUE = 2
    LDA #VALUE
    IF 0
        LDA $10
    ENDIF
    REPT 0
        LDA $11
    ENDM
    RTS
END
""")
        self.assertEqual(len(records), 3)
        self.assertEqual([r["operand_value"] for r in records[:2]], [1, 2])
        self.assertEqual([r["referenced_symbols"] for r in records[:2]], [["VALUE"], ["VALUE"]])

    def test_truncation_parity(self):
        records = self.assemble(".ORG $C000\nLDA #$123\nLDA #(-1)\nRTS\nEND\n")
        self.assertEqual(records[0]["expression"]["value"], 0x123)
        self.assertEqual(records[0]["operand_value"], 0x23)
        self.assertEqual(records[0]["bytes"], [0xA9, 0x23])

    def test_same_include_basename_resolves_to_distinct_sources(self):
        (self.root / "left").mkdir()
        (self.root / "right").mkdir()
        records = self.assemble('.ORG $C000\n.INCSRC "left/outer.asm"\n.INCSRC "right/outer.asm"\nEND\n', includes={
            "left/outer.asm": '.INCSRC "inner.asm"\n', "left/inner.asm": 'LDA #1\n',
            "right/outer.asm": '.INCSRC "inner.asm"\n', "right/inner.asm": 'LDA #2\n',
        })
        self.assertEqual([r["source"]["text"] for r in records], ["LDA #1", "LDA #2"])
        self.assertNotEqual(records[0]["source"]["span"]["file"], records[1]["source"]["span"]["file"])

    def test_invalid_input_and_output_fail(self):
        source = self.root / "input.asm"
        xref = self.root / "xref.json"
        source.write_text(".ORG $C000\nLDA Missing\nEND\n")
        args = [str(XASM), "--pure-binary", "--xref-instructions=true", f"--xref={xref}",
                str(source), "-o", str(self.root / "out.bin")]
        run = subprocess.run(args, capture_output=True)
        self.assertNotEqual(run.returncode, 0)
        self.assertFalse(xref.exists())
        source.write_text(".ORG $C000\nRTS\nEND\n")
        xref.mkdir()
        run = subprocess.run(args, capture_output=True)
        self.assertEqual(run.returncode, 3, run.stderr.decode())

    def test_indexed_load_store_mode_lookup(self):
        records = self.assemble(".ORG $C000\nLDX $10,Y\nSTX $10,Y\nLDX $C000,Y\nEND\n")
        self.assertEqual([r["opcode"] for r in records], [0xB6, 0x96, 0xBE])
        self.assertEqual([r["addressing_mode"] for r in records], ["zeropage_y", "zeropage_y", "absolute_y"])

    def test_swap_parens(self):
        records = self.assemble(".ORG $C000\nLDA ($10),Y\nLDA ($10,X)\nEND\n", options=("--swap-parens",))
        self.assertEqual([r["operand_source"]["text"] for r in records], ["($10),Y", "($10,X)"])
        self.assertEqual([r["addressing_mode"] for r in records], ["postindexed_indirect", "preindexed_indirect"])

    def test_sizeof_strings_datatypes_and_unary_operators(self):
        records = self.assemble('''.ORG $C000
    LDA #sizeof("abc")
    LDX #sizeof(word)
    LDA #~0
    LDA #!0
    LDA #-(1)
    LDA #>$C000
    LDA #'A'
END
''')
        self.assertEqual(records[0]["expression"]["kind"], "sizeof")
        self.assertEqual(records[0]["expression"]["children"][0]["kind"], "string")
        self.assertEqual(records[1]["expression"]["children"][0]["name"], "word")
        self.assertEqual([r["expression"]["operator"] for r in records[2:6]],
                         ["bit_not", "logical_not", "negate", "high_byte"])
        self.assertEqual(records[6]["expression"]["source"]["text"], "'A'")

    def test_offsets_skip_data_storage_and_binary(self):
        (self.root / "payload.bin").write_bytes(b"\x00\x01\x02")
        records = self.assemble('''.ORG $C000
    LDA #1
    .DB 0,1
    .DSB 3
    .INCBIN "payload.bin"
    RTS
END
''')
        self.assertEqual([r["output_offset"] for r in records], [0, 10])
        self.assertEqual([r["cpu_address"] for r in records], [0xC000, 0xC00A])

    def test_nested_macro_and_while_preserve_distinct_operands(self):
        records = self.assemble('''MACRO INNER value
    LDA #value
ENDM
MACRO OUTER argument
    INNER argument
ENDM
.ORG $C000
    OUTER $03
    OUTER $04
COUNT = 2
    WHILE COUNT > 0
        LDA #COUNT
COUNT = COUNT - 1
    ENDM
END
''')
        self.assertEqual([r["operand_value"] for r in records], [3, 4, 2, 1])
        self.assertEqual([r["expression"]["source"]["text"] for r in records[:2]], ["$03", "$04"])
        self.assertEqual(records[2]["source"], records[3]["source"])

    def test_utf8_and_embedded_nul_are_lossless(self):
        for literal in [b"a\x00b", "café".encode(), "λ🙂".encode()]:
            with self.subTest(literal=literal):
                records = self.assemble(b'.ORG $C000\nLDA #sizeof("' + literal + b'")\nRTS\nEND\n')
                self.assertEqual(len(records), 2)
                self.assert_source_spans(records)
                self.assertIn(literal.decode(), records[0]["source"]["text"])
                string = records[0]["expression"]["children"][0]
                self.assertEqual(string["kind"], "string")
                self.assertEqual(string["source"]["text"], '"' + literal.decode() + '"')
                parsed_value = literal.split(b"\x00", 1)[0]
                self.assertEqual(string["name"], parsed_value.decode())
                self.assertEqual(records[0]["operand_value"], len(parsed_value))

    def test_invalid_utf8_is_explicit_analysis_failure(self):
        for instruction in [b"LDA #'\xff'", b'LDA #sizeof("caf\xe9")',
                            b'LDA #sizeof("\xc0\x80")', b'LDA #sizeof("\xed\xa0\x80")',
                            b'LDA #sizeof("\xf4\x90\x80\x80")', b'LDA #sizeof("\xe2\x82")']:
            with self.subTest(instruction=instruction):
                source = self.root / "input.asm"
                source.write_bytes(b'.ORG $C000\n' + instruction + b'\nRTS\nEND\n')
                xref = self.root / "xref.json"
                out = self.root / "out.bin"
                args = [str(XASM), "--pure-binary", f"--xref={xref}", str(source), "-o", str(out)]
                plain = subprocess.run(args, capture_output=True)
                self.assertEqual(plain.returncode, 0, plain.stderr.decode())
                json.loads(xref.read_bytes())
                expected = out.read_bytes()
                run = subprocess.run([*args, "--xref-instructions=true"], capture_output=True)
                self.assertEqual(run.returncode, 3, run.stderr.decode())
                self.assertIn(b"instruction source span is not UTF-8", run.stderr)
                self.assertEqual(out.read_bytes(), expected)

    def test_linker_indexed_shortening_and_wide_operands(self):
        source = self.root / "input.asm"
        source.write_text('''.DATASEG
Scratch .DSB 1
.CODESEG
    LDX Scratch,Y
    STX Scratch,Y
    LDX Scratch+$100,Y
    LDX.W Scratch,Y
END
''')
        obj = self.root / "input.o"
        output = self.root / "linked.bin"
        run = subprocess.run([str(XASM), str(source), "-o", str(obj)], capture_output=True)
        self.assertEqual(run.returncode, 0, run.stderr.decode())
        script = self.root / "link.script"
        script.write_text(f"output{{file={output}}}\nram{{start=$10,end=$FF}}\nbank{{size=$10,origin=$C000}}\nlink{{file={obj}}}\n")
        run = subprocess.run([str(XLNK), str(script)], capture_output=True)
        self.assertEqual(run.returncode, 0, run.stderr.decode())
        self.assertEqual(output.read_bytes()[:10], bytes([0xB6, 0x10, 0x96, 0x10, 0xBE, 0x10, 1, 0xBE, 0x10, 0]))

    def test_cli_refusal(self):
        source = self.root / "input.asm"
        source.write_text(".ORG $C000\nRTS\nEND\n")
        for args in [[], ["--xref=out.json"], ["--pure-binary"],
                     ["--pure-binary", "--xref=out.json", "--xref-format=csv"],
                     ["--pure-binary", "--xref=out.json", "--xref-format=text"]]:
            with self.subTest(args=args):
                run = subprocess.run([str(XASM), *args, "--xref-instructions=true", str(source)], cwd=self.root, capture_output=True)
                self.assertEqual(run.returncode, 2, run.stderr.decode())
                self.assertFalse((self.root / "out.json").exists())

        run = subprocess.run([str(XASM), "--xref-instructions=maybe", str(source)], capture_output=True)
        self.assertEqual(run.returncode, 2)


    def test_sidecar_cli_refusal(self):
        source = self.root / "input.asm"
        source.write_text(".ORG $C000\nRTS\n")
        cases = [([], "requires --dependency-manifest"),
                 (["--dependency-manifest=deps.json"], "requires --pure-binary"),
                 (["--pure-binary", "--dependency-manifest=deps.json", "--xref=refs.json", "--xref-format=csv"], "JSON xref"),
                 (["--pure-binary", "--dependency-manifest=deps.json", "--xref-instructions=true"], "requires --xref=FILE")]
        for options, message in cases:
            with self.subTest(options=options):
                run = subprocess.run([str(XASM), *options, "--instruction-records-output=instructions.json", str(source)],
                                     cwd=self.root, capture_output=True)
                self.assertEqual(run.returncode, 2, run.stderr)
                self.assertIn(message.encode(), run.stderr)
                self.assertFalse((self.root / "instructions.json").exists())
                self.assertFalse((self.root / "deps.json").exists())
        run = subprocess.run([str(XASM), "--instruction-records-output=", str(source)], capture_output=True)
        self.assertEqual(run.returncode, 2)
        self.assertIn(b"nonempty filename", run.stderr)

    def test_sidecar_collisions_refused_before_any_output(self):
        source = self.root / "input.asm"
        include = self.root / "part.asm"
        binary = self.root / "data.bin"
        source.write_text('.ORG $C000\n.INCSRC "part.asm"\n.INCBIN "data.bin"\n')
        include.write_text("RTS\n")
        binary.write_bytes(b"\x01")
        alias = self.root / "source-alias.asm"
        os.link(source, alias)
        inputs = {path: path.read_bytes() for path in (source, include, binary, alias)}
        output, xref, listing, manifest = [self.root / name for name in ("out.bin", "xref.json", "listing.json", "deps.json")]
        for target in (source, include, binary, alias, output, xref, listing, manifest):
            with self.subTest(target=target.name):
                run = subprocess.run([str(XASM), "--pure-binary", "-o", str(output), f"--xref={xref}",
                                      f"--listing={listing}", "--listing-format=json", f"--dependency-manifest={manifest}",
                                      f"--instruction-records-output={target}", str(source)], capture_output=True)
                self.assertEqual(run.returncode, 3, run.stderr)
                self.assertIn(b"aliases an input, lookup probe, or another output", run.stderr)
                for path, original in inputs.items():
                    self.assertEqual(path.read_bytes(), original)
                for path in (output, xref, listing, manifest):
                    self.assertFalse(path.exists(), path)

    def test_sidecar_negative_lookup_collision(self):
        (self.root / "fallback").mkdir()
        (self.root / "fallback/child.inc").write_text("RTS\n")
        source = self.root / "input.asm"
        source.write_text('.ORG $C000\n.INCSRC "child.inc"\n')
        run = subprocess.run([str(XASM), "--pure-binary", "-Ifallback", "-o", "out.bin",
                              "--dependency-manifest=deps.json", "--instruction-records-output=child.inc", str(source)],
                             cwd=self.root, capture_output=True)
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertIn(b"aliases an input, lookup probe, or another output", run.stderr)
        for name in ("child.inc", "out.bin", "deps.json"):
            self.assertFalse((self.root / name).exists())

    def test_sidecar_failure_never_publishes_manifest(self):
        source, output, sidecar, manifest = [self.root / name for name in ("input.asm", "out.bin", "instructions.json", "deps.json")]
        sidecar.mkdir()
        source.write_text(".ORG $C000\nRTS\n")
        args = [str(XASM), "--pure-binary", "-o", str(output), f"--dependency-manifest={manifest}",
                f"--instruction-records-output={sidecar}", str(source)]
        run = subprocess.run(args, capture_output=True)
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertIn(b"could not write output", run.stderr)
        self.assertIn(str(sidecar).encode(), run.stderr)
        self.assertFalse(manifest.exists())
        self.assertEqual(output.read_bytes(), b"\x60")
        sidecar.rmdir()
        source.write_bytes(b".ORG $C000\nLDA #'\xff'\n")
        run = subprocess.run(args, capture_output=True)
        self.assertEqual(run.returncode, 3, run.stderr)
        self.assertIn(b"instruction source span is not UTF-8", run.stderr)
        self.assertFalse(manifest.exists())
        sidecar.write_text('{"version":"1","records":[]}\n')
        original = sidecar.read_bytes()
        source.write_text(".ORG $C000\nNOT_AN_OPCODE\n")
        run = subprocess.run(args, capture_output=True)
        self.assertNotEqual(run.returncode, 0)
        self.assertFalse(manifest.exists())
        self.assertEqual(sidecar.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
