#!/usr/bin/env python3
"""Legacy access classification across xref, summary, index patterns and data consumers."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


XASM = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "xasm"

SOURCE = """.ORG $C000
PTR_LO .EQU <Table
Start:
    LDA #PTR_LO
    LDA #<Table
    STA Ptr
    LDA #>Table
    STA Ptr+1
    LDY #0
    LDA [Ptr],Y
    INC Ptr+1
    STA [Ptr],Y
    LDX #0
    LDA [Ptr,X]
    BIT Flags
    INC Counters,X
    LDX #Zp
    JMP [Vec]
    JSR Sub
    JMP Start
    BNE Start
Sub:
    RTS
Table:
    .DB 1,2,3
Flags:
    .DB 0
Counters:
    .DB 0,0,0,0
Vec:
    .DW Start
.DATASEG
.ORG $0010
Ptr:
    .DW 0
Zp:
    .DB 0
END
"""


class AccessClassification(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="xasm-access-")
        root = Path(cls.temp.name)
        source = root / "input.asm"
        source.write_text(SOURCE)
        outputs = {name: root / f"{name}.json" for name in ("xref", "summary", "index", "consumers")}
        run = subprocess.run([
            str(XASM), "--pure-binary", str(source), "-o", str(root / "output.bin"),
            f"--xref={outputs['xref']}", "--xref-data=true",
            "--xref-summary", f"--xref-summary-output={outputs['summary']}", "--xref-summary-format=json",
            "--analyze-index-patterns", f"--index-patterns-output={outputs['index']}",
            "--index-patterns-format=json",
            "--data-consumers", f"--data-consumers-output={outputs['consumers']}",
            "--data-consumers-format=json",
        ], capture_output=True)
        if run.returncode != 0:
            raise AssertionError(run.stderr.decode())
        cls.outputs = {name: json.loads(path.read_text()) for name, path in outputs.items()}
        listing = [line.strip() for line in SOURCE.splitlines()]
        cls.line = {text: number for number, text in enumerate(listing, 1)}

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def site(self, text):
        """CPU address of the instruction on the source line with this text."""
        for reference in self.outputs["xref"]["references"]:
            if reference["line"] == self.line[text]:
                return int(reference["use_cpu_address"], 16)
        raise AssertionError(f"no reference on line {text!r}")

    def test_reference_access(self):
        self.assertEqual(
            [(r["symbol"], r["opcode"], r["addressing_mode"], r["access"])
             for r in self.outputs["xref"]["references"]],
            [("Table", "LDA", "immediate", "pointer_lo"),
             ("Table", "LDA", "immediate", "pointer_lo"),
             ("Ptr", "STA", "absolute", "write"),
             ("Table", "LDA", "immediate", "pointer_hi"),
             ("Ptr", "STA", "absolute", "write"),
             ("Ptr", "LDA", "postindexed_indirect", "read"),
             ("Ptr", "INC", "absolute", "read_modify_write"),
             ("Ptr", "STA", "postindexed_indirect", "read"),
             ("Ptr", "LDA", "preindexed_indirect", "read"),
             ("Flags", "BIT", "absolute", "read"),
             ("Counters", "INC", "absolute_x", "read_modify_write"),
             ("Zp", "LDX", "immediate", "immediate"),
             ("Vec", "JMP", "indirect", "read"),
             ("Sub", "JSR", "absolute", "call"),
             ("Start", "JMP", "absolute", "jump"),
             ("Start", "BNE", "relative", "branch"),
             ("Start", None, None, "address_compute")])

    def edges(self, section):
        return sorted((edge["symbol"], int(edge["site_addr"], 16), edge["displacement"])
                      for edge in self.outputs["xref"][section])

    def test_read_modify_write_is_a_read_and_a_write_edge(self):
        pointer_step = self.site("INC Ptr+1")
        counters = self.site("INC Counters,X")
        self.assertEqual(self.edges("data_reads"), sorted([
            ("Ptr", pointer_step, 1), ("Flags", self.site("BIT Flags"), 0), ("Counters", counters, 0)]))
        self.assertEqual(self.edges("data_writes"), sorted([
            ("Ptr", self.site("STA Ptr"), 0), ("Ptr", self.site("STA Ptr+1"), 1),
            ("Ptr", pointer_step, 1), ("Counters", counters, 0)]))

    def test_read_modify_write_keeps_pointer_setup(self):
        producer = "0x%04X" % self.site("STA Ptr+1")
        flows = [(flow["producer_site"], flow["consumer_site"], flow["access_kind"])
                 for flow in self.outputs["xref"]["indirect_data_flows"]]
        self.assertEqual(flows, [
            (producer, "0x%04X" % self.site("LDA [Ptr],Y"), "read"),
            (producer, "0x%04X" % self.site("STA [Ptr],Y"), "write"),
            (producer, "0x%04X" % self.site("LDA [Ptr,X]"), "read")])

    def test_summary_counts(self):
        summary = self.outputs["summary"]
        data = {entry["label"]: (entry["read_count"], entry["write_count"], entry["total_ref_count"])
                for entry in summary["top_data_labels"]}
        self.assertEqual(data, {"Ptr": (4, 3, 6), "Flags": (1, 0, 1), "Counters": (1, 1, 1),
                                "Vec": (1, 0, 1)})
        self.assertEqual([entry["label"] for entry in summary["top_jump_targets"]], ["Start"])
        self.assertEqual([entry["label"] for entry in summary["top_callables"]], ["Sub"])

    def test_index_pattern_for_read_modify_write(self):
        records = [(r["table_label"], int(r["site_addr"], 16), r["access_kind"], r["access_pattern"],
                    r.get("evidence_flags", [])) for r in self.outputs["index"]]
        self.assertEqual(records, [("Counters", self.site("INC Counters,X"), "read_modify_write",
                                    "base", ["write_access"])])

    def test_data_consumer_sites(self):
        consumers = {entry["label"]: entry for entry in self.outputs["consumers"]}
        counters = consumers["Counters"]
        site = self.site("INC Counters,X")
        self.assertEqual([int(s["site_addr"], 16) for s in counters["read_sites"]], [site])
        self.assertEqual([int(s["site_addr"], 16) for s in counters["write_sites"]], [site])
        self.assertEqual((counters["read_site_count"], counters["write_site_count"]), (1, 1))
        flags = consumers["Flags"]
        self.assertEqual([int(s["site_addr"], 16) for s in flags["read_sites"]], [self.site("BIT Flags")])
        self.assertEqual(flags["write_sites"], [])


if __name__ == "__main__":
    unittest.main()
