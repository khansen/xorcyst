#!/usr/bin/env python3
"""Public lossless model: native fixtures and independent saved-model mutations."""
import base64
import copy
import json
import os
import re
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from source_model_support import ModelError, reconstruct, validate, sha, hash_fields
from mos6502_oracle import OPCODES
from source_model_schema import check as check_schema

SCHEMA = json.loads((Path(__file__).resolve().parents[1] / "doc/source-model.schema.json").read_bytes())

XASM = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "xasm"


class SourceModelTests(unittest.TestCase):
    def assemble(self, source, options=(), expected=0):
        with tempfile.TemporaryDirectory(prefix="xasm-source-model-") as directory:
            root = Path(directory)
            (root / "input.asm").write_bytes(source)
            command = [str(XASM), "--pure-binary", *options, "input.asm", "-o", "output.bin"]
            plain = subprocess.run(command, cwd=root, capture_output=True)
            result = subprocess.run([*command, "--source-model=model.json",
                                     "--source-model-diagnostics=diagnostic.json"], cwd=root, capture_output=True)
            self.assertEqual(result.returncode, expected, (source, result.stderr.decode(errors="replace")))
            selected = root / ("model.json" if expected in (0, 4, 5) else "diagnostic.json")
            self.assertTrue(selected.exists(), result.stderr)
            model = json.loads(selected.read_bytes())
            check_schema(model, SCHEMA)
            if model["status"] == "certified":
                self.assertEqual(plain.returncode, expected)
                self.assertEqual(plain.stderr, result.stderr, "capture changed native warnings")
            # The checker has no source, binary or checkout to fall back to.
            (root / "input.asm").unlink()
            if (root / "output.bin").exists():
                (root / "output.bin").unlink()
            self.assertEqual(validate(model), {"input.asm": source})
            return model

    def test_exact_bytes_and_native_grammar(self):
        accepted = [
            (b"NOP\n", b"\xea"), (b"\tnOp\n\n; utf8 \xc3\xa6\n", b"\xea"),
            (b"\n", b""), (b"NOP ; c\r\n", b"\xea"),
            (b'.DB "\xff\xfe"\n', b"\xff\xfe"), (b"NOP\n.END\n", b"\xea"),
        ]
        for source, output in accepted:
            with self.subTest(source=source):
                model = self.assemble(source)
                self.assertEqual(base64.b64decode(model["binary"]["bytes_base64"]), output)
                if b".END" in source:
                    self.assertEqual(sum(s["kind"] == "END_NODE" for s in model["syntax"]), 1)
        for source in (b"", b"NOP", b"; comment", b"NOP\r\n", b"\xef\xbb\xbfNOP\n",
                       b".END\n\n", b".END\n; tail\n", b"NOP\n.END\n\n", b"NOP\n.END\n; tail\n"):
            with self.subTest(source=source):
                model = self.assemble(source, expected=1)
                self.assertEqual(model["status"], "diagnostic")

    def test_token_and_form_coverage(self):
        model = self.assemble(b"ORG $8000\nFoo: DB 1\n.WORD Foo\n.byte 2\nEND\n")
        self.assertEqual(base64.b64decode(model["binary"]["bytes_base64"]), b"\x01\x00\x80\x02")
        refused = [b".DEFINE V 7\n", b".DEFINE F\n", b".DB\n", b".DB [3]\n", b"Foo .DB 1\n",
                   b".DSB 3\n", b".CHAR 2\n", b".DWORD 1\n", b".IF 0\nNOP\n.ENDIF\n",
                   b"V = 7\n", b".DB ^V\nV: NOP\n", b"V .EQU \"x\"\n", b".DW \"x\"\n"]
        for source in refused:
            with self.subTest(source=source):
                model = self.assemble(source, expected=1)
                self.assertIn("SOURCE_MODEL_UNSUPPORTED_CONSTRUCT", {d["code"] for d in model["diagnostics"]})

    def test_anonymous_equ_refusals(self):
        for source in (
            b".ORG $8000\nV .EQU +\nNOP\n+ NOP\nJMP V\n+ NOP\n",
            b".ORG $8000\n- NOP\nV .EQU -\n- NOP\nJMP V\n- NOP\nJMP V\n",
            b"V .EQU +\n",
        ):
            for options in ((), ("--xref=x.json",), ("--listing=l.txt",)):
                with self.subTest(source=source, options=options):
                    model = self.assemble(source, options, expected=1)
                    self.assertIn("SOURCE_MODEL_UNSUPPORTED_CONSTRUCT", {d["code"] for d in model["diagnostics"]})

    def test_diagnostic_consumed_files_with_equal_basenames(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "sub").mkdir()
            (root / "input.asm").write_bytes(b'.INCLUDE "sub/input.asm"\nNOP\n')
            (root / "sub/input.asm").write_bytes(b"NOP\n")
            result = subprocess.run([str(XASM), "--pure-binary", "input.asm", "-o", "out",
                                     "--source-model=model", "--source-model-diagnostics=diag"], cwd=root, capture_output=True)
            self.assertEqual(result.returncode, 1, result.stderr)
            model = json.loads((root / "diag").read_bytes())
            (root / "input.asm").unlink()
            (root / "sub/input.asm").unlink()
            check_schema(model, SCHEMA)
            self.assertEqual(validate(model), {"input.asm": b'.INCLUDE "sub/input.asm"\nNOP\n', "f2/input.asm": b"NOP\n"})

    def test_unused_equ_resolution_and_warnings(self):
        for source in (b"V .EQU Missing+1\n", b"V .EQU @@none\n", b"V .EQU W\nW .EQU V\n"):
            for options in ((), ("--xref=x.json",), ("--listing=l.txt",)):
                with self.subTest(source=source, options=options):
                    model = self.assemble(source, options, expected=1)
                    expected = "SOURCE_MODEL_VALUE_UNAVAILABLE" if b"W .EQU V" in source else "SOURCE_MODEL_UNBOUND_REFERENCE"
                    self.assertIn(expected, {d["code"] for d in model["diagnostics"]})
        for tail in (b"", b"LDA #V\n"):
            model = self.assemble(b"Cell .EQU 7\nV .EQU Cell+1\n" + tail, ("--warn-unused-equ",))
            ref = next(r for r in model["references"] if r["name"] == "Cell")
            declaration = next(d for d in model["declarations"] if d["id"] == ref["declaration_id"])
            self.assertEqual(declaration["name"], "Cell")
            values = {v["expression_id"]: v["value"] for v in model["evaluations"]}
            v = next(d for d in model["declarations"] if d["name"] == "V")
            self.assertEqual(values[v["expression_id"]], {"kind": "integer", "value": 8})

    def test_duplicate_symbols(self):
        for source in (b"V .EQU 7\nV .EQU 7\n", b"V .EQU 7\nV .EQU 8\n",
                       b"V: NOP\nV .EQU 7\n", b"V .EQU 7\nV: NOP\n"):
            model = self.assemble(source, expected=1)
            issue = next(d for d in model["diagnostics"] if d["code"] == "SOURCE_MODEL_DUPLICATE_DECLARATION")
            self.assertIsNotNone(issue["related_declaration_id"])

    def test_all_bindings_and_pc_contexts(self):
        source = b""".ORG $8000
Cell .EQU $10
MaskA .EQU 1
MaskB .EQU 2
Stride .EQU 4
Field .EQU 1
Alias .EQU Cell+1
Forward .EQU Later
First: LDA #(MaskA|MaskB)
STA Cell+(Stride*2)+Field,X
@@loop: BNE @@loop
Ptr .EQU @@loop
Distance .EQU Later-$
.DW First,Alias,Ptr,$,$
Second: NOP
@@loop: BNE @@loop
.DW Ptr,Distance
Later: NOP
"""
        model = self.assemble(source)
        self.assertEqual(model["coverage"]["expected_references"], len(model["references"]))
        self.assertEqual(model["coverage"]["bound_references"], len(model["references"]))
        self.assertEqual(model["instructions"][1]["memory_access"]["data"]["kind"], "write")

    def test_native_data_ranges_and_wide_suffix(self):
        source = b'.ORG $8000\n.DB "",1,"ab",<Entry\n.DW $,$,Entry\nEntry: DEX.W\nLDA.W #1\nLDA.W $10\n'
        model = self.assemble(source)
        self.assertEqual(base64.b64decode(model["binary"]["bytes_base64"]),
                         bytes.fromhex("0161620a048006800a80caa901ad1000"))
        self.assertEqual([r["forced_absolute"] for r in model["instructions"]], [True, True, True])
        debug = self.assemble(source, ("--debug",))
        for table in ("syntax", "expressions", "references", "data", "emissions", "locations"):
            self.assertEqual(model[table], debug[table], table)

    def test_segments_and_implicit_origin(self):
        for source, output in (
            (b"NOP\nL: JMP L\n", "ea4c0100"),
            (b"L: .ORG $8000\nNOP\n.DW L\n", "ea0000"),
            (b".ORG $8000\nAlpha: NOP\nBeta: .ORG $8000\nGamma: NOP\n.DW Alpha,Beta,Gamma\n", "eaea008001800080"),
        ):
            model = self.assemble(source)
            self.assertEqual(base64.b64decode(model["binary"]["bytes_base64"]), bytes.fromhex(output))

    def assert_mutation(self, model, mutate, code):
        damaged = copy.deepcopy(model)
        mutate(damaged)
        with self.assertRaisesRegex(ModelError, "^" + code + "$"):
            validate(damaged)

    def test_model_mutations(self):
        model = self.assemble(b".ORG $8000\nOne .EQU 8\nOther .EQU 8\nFirst: LDA #One\n@@loop: BNE @@loop\nSecond: NOP\n@@loop: BNE @@loop\n.DW First,Second\n")
        other = next(d["id"] for d in model["declarations"] if d["name"] == "Other")
        self.assert_mutation(model, lambda m: m["references"][0].update(declaration_id=other), "MODEL_BINDING_TARGET")
        locals_ = [d["id"] for d in model["declarations"] if d["kind"] == "local"]
        self.assert_mutation(model, lambda m: next(r for r in m["references"] if r["name"] == "@@loop").update(declaration_id=locals_[1]), "MODEL_BINDING_TARGET")
        self.assert_mutation(model, lambda m: m["references"].pop(), "MODEL_REFERENCE_COVERAGE")
        self.assert_mutation(model, lambda m: m["references"].append(m["references"][0]), "MODEL_REFERENCE_COVERAGE")
        self.assert_mutation(model, lambda m: m["units"].pop(), "MODEL_SOURCE_PARTITION")
        self.assert_mutation(model, lambda m: m["files"][0].update(bytes_base64="eA=="), "MODEL_SOURCE_HASH")
        self.assert_mutation(model, lambda m: m["locations"][-1].update(segment_id="g1"), "MODEL_SEGMENT")
        self.assert_mutation(model, lambda m: m["data"][0]["operands"][0].update(output_offset=0), "MODEL_EMISSION_RANGE")
        self.assert_mutation(model, lambda m: m["evaluations"][0]["value"].update(value=123), "MODEL_VALUE")
        self.assert_mutation(model, lambda m: m["identity"].update(input_id="0"*64), "MODEL_IDENTITY")

    def test_consistently_corrupted_value_and_binary(self):
        model = self.assemble(b"Cell .EQU 7\nLDA #Cell\n")
        def corrupt(m):
            expr = next(r["expression_id"] for r in m["references"] if r["name"] == "Cell")
            next(v for v in m["evaluations"] if v["expression_id"] == expr)["value"]["value"] = 9
            raw = bytes.fromhex("a909")
            m["binary"].update(bytes_base64=base64.b64encode(raw).decode(), sha256=sha(raw))
        self.assert_mutation(model, corrupt, "MODEL_VALUE")

    def test_inventory_is_independent_of_export_counts(self):
        model = self.assemble(b"Cell .EQU 7\nAlias .EQU Cell+1\n.DW Alias\n")
        def drop_leaf(m):
            ref = m["references"].pop()
            m["expressions"] = [e for e in m["expressions"] if e["id"] != ref["expression_id"]]
            m["coverage"]["expected_references"] -= 1
            m["coverage"]["bound_references"] -= 1
            m["coverage"]["expressions"] -= 1
        self.assert_mutation(model, drop_leaf, "MODEL_REFERENCE_COVERAGE")
        self.assert_mutation(model, lambda m: m["data"][0]["operands"][0]["emission_ranges"].clear(), "MODEL_EMISSION_RANGE")

    def test_anonymous_exact_spelling(self):
        model = self.assemble(b".ORG $8000\nJMP +\n++ NOP\n+ NOP\n- NOP\n-- NOP\nJMP -\nJMP --\n")
        self.assertEqual(base64.b64decode(model["binary"]["bytes_base64"]), bytes.fromhex("4c0480eaeaeaea4c05804c0680"))
        wrong = next(d for d in model["declarations"] if d["name"] == "++")
        def redirect(m):
            m["references"][0]["declaration_id"] = wrong["id"]
            v = next(v for v in m["evaluations"] if v["expression_id"] == m["references"][0]["expression_id"])
            v["value"]["value"] = 0x8003
            raw = bytearray(base64.b64decode(m["binary"]["bytes_base64"]))
            raw[1] = 3
            m["binary"]["bytes_base64"] = base64.b64encode(raw).decode()
            m["binary"]["sha256"] = sha(raw)
        self.assert_mutation(model, redirect, "MODEL_BINDING_TARGET")

    def test_case_and_paren_options(self):
        model = self.assemble(b"Value .EQU $10\nStart: LDA value\nJMP START\n", ("--case-insensitive",))
        self.assertEqual([r["spelling"] for r in model["references"]], ["value", "START"])
        self.assertEqual([r["name"] for r in model["references"]], ["VALUE", "START"])
        model = self.assemble(b"LDA ($10),Y\nLDA [$10],Y\n", ("--swap-parens",))
        self.assertEqual(base64.b64decode(model["binary"]["bytes_base64"]), bytes.fromhex("b110b91000"))
        self.assemble(b"Value .EQU 1\nVALUE .EQU 1\n", ("--case-insensitive",), 1)

    def test_old_products_and_capture_independence(self):
        source = b".ORG $8000\nUnused .EQU 7\nCell .EQU $10\nAlias .EQU Cell+1\nEntry: LDA #Alias\nSTA Cell,X\n.DW Entry\n.DB 1,2\n.DB 3\n"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); (root / "input.asm").write_bytes(source)
            options = ["--pure-binary", "--xref=xref.json", "--xref-data=true", "--xref-instructions=true",
                       "--instruction-records-output=instructions.json", "--dependency-manifest=dependencies.json",
                       "--listing=listing.json", "--listing-format=json", "--warn-unused-equ"]
            command = [str(XASM), *options, "input.asm", "-o", "output.bin"]
            plain = subprocess.run(command, cwd=root, capture_output=True)
            self.assertEqual(plain.returncode, 0, plain.stderr)
            names = ("output.bin", "xref.json", "instructions.json", "listing.json")
            def products():
                result = {name: (root / name).read_bytes() for name in names}
                # The old xref records wall-clock time; compare every other byte.
                result["xref.json"] = re.sub(rb'("timestamp_utc"\s*:\s*)"[^"]*"', rb'\1"<clock>"', result["xref.json"])
                return result
            before = products()
            result = subprocess.run([*command, "--source-model=model.json"], cwd=root, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(plain.stderr, result.stderr)
            self.assertEqual(before, products())
            combined = json.loads((root / "model.json").read_bytes())
            alone = self.assemble(source)
            for table in ("syntax", "units", "scopes", "declarations", "expressions", "references",
                          "evaluations", "segments", "locations", "data", "instructions", "emissions"):
                self.assertEqual(alone[table], combined[table], table)

    def test_memory_access_for_every_official_opcode(self):
        syntax = {
            "implied": "{m}", "accumulator": "{m} A", "immediate": "{m} #$12",
            "zeropage": "{m} $12", "zeropage_x": "{m} $12,X", "zeropage_y": "{m} $12,Y",
            "absolute": "{m} $1234", "absolute_x": "{m} $1234,X", "absolute_y": "{m} $1234,Y",
            "preindexed_indirect": "{m} [$12,X]", "postindexed_indirect": "{m} [$12],Y",
            "indirect": "{m} [$1234]", "relative": "{m} $+2",
        }
        codes = sorted(OPCODES)
        lines = [syntax[OPCODES[code][1]].format(m=OPCODES[code][0]) for code in codes]
        model = self.assemble((".ORG $C000\n" + "".join(f"    {line}\n" for line in lines) + "END\n").encode())
        self.assertEqual([r["opcode"] for r in model["instructions"]], codes)

    def test_resource_depth_refusal(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "input.asm").write_bytes(b".DB " + b"1+" * 520 + b"1\n")
            (root / "out").write_bytes(b"old binary")
            (root / "model").write_bytes(b"old model")
            result = subprocess.run([str(XASM), "--pure-binary", "input.asm", "-o", "out", "--source-model=model"],
                                    cwd=root, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"SOURCE_MODEL_RESOURCE_LIMIT", result.stderr)
            self.assertEqual((root / "out").read_bytes(), b"old binary")
            self.assertEqual((root / "model").read_bytes(), b"old model")

    def test_cli_and_path_refusals(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); source = root / "input.asm"; source.write_bytes(b"NOP\n")
            for options in (["--source-model="], ["--source-model=model", "--source-model-profile=unknown"],
                            ["--source-model-profile=asm-edit-v1"], ["--source-model-diagnostics=diag"],
                            ["--source-model=model", "-DValue=1"]):
                result = subprocess.run([str(XASM), "--pure-binary", *options, "input.asm"], cwd=root, capture_output=True)
                self.assertEqual(result.returncode, 2, (options, result.stderr))
            result = subprocess.run([str(XASM), "--source-model=model", "input.asm"], cwd=root, capture_output=True)
            self.assertEqual(result.returncode, 2)
            for options in (["--source-model=input.asm"],
                            ["--source-model=model", "--source-model-diagnostics=model"],
                            ["--source-model=out", "-o", "out"]):
                result = subprocess.run([str(XASM), "--pure-binary", *options, "input.asm"], cwd=root, capture_output=True)
                self.assertEqual(result.returncode, 3, (options, result.stderr))
                self.assertEqual(source.read_bytes(), b"NOP\n")
            source.write_bytes(b"bad source\n")
            (root / "out").write_bytes(b"existing")
            result = subprocess.run([str(XASM), "--pure-binary", "--source-model=model",
                                     "--source-model-diagnostics=out", "input.asm", "-o", "out"], cwd=root, capture_output=True)
            self.assertEqual(result.returncode, 3, result.stderr)
            self.assertEqual((root / "out").read_bytes(), b"existing")

    def test_outcomes_and_repair(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); source = root / "input.asm"
            source.write_bytes(b".ORG $8000\nL: NOP\nJMP $8000\nJMP L\n")
            (root / "ref.bin").write_bytes(bytes.fromhex("ea4c00804c0080"))
            command = [str(XASM), "--pure-binary", "--source-model=model", "--source-model-diagnostics=diag",
                       "--compare=ref.bin", "input.asm", "-o", "out"]
            result = subprocess.run([*command, "--audit-raw-addresses", "--audit-level=error"], cwd=root, capture_output=True)
            self.assertEqual(result.returncode, 4, result.stderr)
            model = json.loads((root / "model").read_bytes()); validate(model)
            self.assertEqual(model["analysis_outcomes"], {"comparison": "not_run", "audit": "findings"})
            self.assertNotEqual(model["analysis_outcomes"]["comparison"], "equal")
            result = subprocess.run(command, cwd=root, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            good = (root / "model").read_bytes()
            self.assertEqual(json.loads(good)["analysis_outcomes"]["comparison"], "equal")
            source.write_bytes(b"NOP")
            result = subprocess.run(command, cwd=root, capture_output=True)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertEqual((root / "model").read_bytes(), good)
            validate(json.loads((root / "diag").read_bytes()))
            source.write_bytes(b"CLC\n")
            result = subprocess.run(command, cwd=root, capture_output=True)
            self.assertEqual(result.returncode, 5, result.stderr)
            model = json.loads((root / "model").read_bytes()); validate(model)
            self.assertEqual(model["analysis_outcomes"]["comparison"], "mismatch")

    def test_canonical_identity_vectors_and_saved_example(self):
        folder = Path(__file__).resolve().parent / "fixtures/source-model"
        for vector in json.loads((folder / "identity.json").read_bytes()):
            self.assertEqual(sha(bytes.fromhex(vector["wire_hex"])), vector["sha256"])
            self.assertEqual(hash_fields(vector["fields"]), vector["sha256"])
        example = json.loads((folder / "example.json").read_bytes())
        check_schema(example, SCHEMA)
        self.assertEqual(validate(example), {"example.asm": (folder / "example.asm").read_bytes()})

    def test_relocated_identity(self):
        one = self.assemble(b".ORG $8000\nEntry: JMP Entry\n")
        two = self.assemble(b".ORG $8000\nEntry: JMP Entry\n")
        self.assertEqual(one["identity"], two["identity"])
        for key in ("syntax", "units", "references", "declarations", "locations"):
            self.assertEqual(one[key], two[key])
        different = self.assemble(b".ORG $8000\nEntry: JMP Entry\n", ("--case-insensitive",))
        self.assertNotEqual(one["identity"]["input_id"], different["identity"]["input_id"])


if __name__ == "__main__":
    unittest.main()
