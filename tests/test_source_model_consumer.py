#!/usr/bin/env python3
"""Neutral consumer migration: saved model -> bound edit -> refresh -> review."""
import base64
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from source_model_support import validate

XASM = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "xasm"


class ConsumerTest(unittest.TestCase):
    def test_bound_edit_refresh_and_review_without_original(self):
        source = b".ORG $8000\nCell .EQU $10\nField .EQU 1\nEntry: LDA Cell+Field,X\n@@loop: BNE @@loop\n.DW Entry\n"
        with tempfile.TemporaryDirectory(prefix="xasm-model-consumer-") as directory:
            root = Path(directory)
            original = root / "original.asm"
            original.write_bytes(source)
            subprocess.run([str(XASM), "--pure-binary", str(original), "-o", str(root / "out"),
                            "--source-model=" + str(root / "before")], check=True, capture_output=True)
            before = json.loads((root / "before").read_bytes())
            original.unlink()
            (root / "out").unlink()
            saved = validate(before)
            self.assertEqual(saved, {"original.asm": source})
            declaration = next(d for d in before["declarations"] if d["name"] == "Cell")
            references = [r for r in before["references"] if r["declaration_id"] == declaration["id"]]
            self.assertEqual(len(references), 1)
            spans = [declaration["name_span"], *(r["span"] for r in references)]
            edited = saved["original.asm"]
            for span in sorted(spans, key=lambda x: x["byte_start"], reverse=True):
                edited = edited[:span["byte_start"]] + b"ObjectSlots" + edited[span["byte_end_exclusive"]:]
            refreshed = root / "candidate.asm"
            refreshed.write_bytes(edited)
            (root / "reference").write_bytes(base64.b64decode(before["binary"]["bytes_base64"]))
            subprocess.run([str(XASM), "--pure-binary", str(refreshed), "-o", str(root / "out"),
                            "--compare=" + str(root / "reference"), "--source-model=" + str(root / "after")],
                           check=True, capture_output=True)
            after = json.loads((root / "after").read_bytes())
            refreshed.unlink()
            self.assertEqual(validate(after), {"candidate.asm": edited})
            self.assertEqual(after["analysis_outcomes"]["comparison"], "equal")
            self.assertEqual(before["binary"], after["binary"])
            old_decls = {d["id"]: d for d in before["declarations"]}
            review = [{"declaration_id": d["id"], "before": old_decls[d["id"]]["name"], "after": d["name"]}
                      for d in after["declarations"] if d["name"] != old_decls[d["id"]]["name"]]
            self.assertEqual(review, [{"declaration_id": declaration["id"], "before": "Cell", "after": "ObjectSlots"}])
            self.assertEqual([(r["id"], r["declaration_id"]) for r in before["references"]],
                             [(r["id"], r["declaration_id"]) for r in after["references"]])
            # Review is derived from the models and recorded selection; no
            # raw source, original checkout or acceptance verdict is consulted.
            self.assertFalse(original.exists())


if __name__ == "__main__":
    unittest.main()
