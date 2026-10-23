#!/usr/bin/env python3
"""Summarize a completed local capture; inventory frozen consumer dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

from freeze_baselines import digest, manifest, save


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("capture", type=Path)
    p.add_argument("--nesrev", type=Path, required=True)
    p.add_argument("--gate", type=Path, required=True)
    p.add_argument("--wire-review", type=Path)
    p.add_argument("--sizeof-capture", type=Path)
    args = p.parse_args()
    root = args.capture.resolve()
    report = json.loads((root / "capture.json").read_text())
    if not report.get("complete"):
        raise SystemExit("capture is incomplete")
    local = Path(__file__).resolve().parent
    out = local / "baseline-index.json"
    index = {k: report[k] for k in ("baseline", "baseline_commit", "platform", "machine",
             "compiler", "source_archive", "tools", "nesrev_commit", "nesrev_manifest_sha256")}
    index["capture_root"] = str(root)
    index["capture_sha256"] = digest(root / "capture.json")["sha256"]
    index["projects"] = {}
    for name, value in report["projects"].items():
        index["projects"][name] = {k: value[k] for k in
                                  ("source", "build_directory", "target", "product",
                                   "manifest_sha256", "files")}
        index["projects"][name].update(value["runs"][0]["product"])
        index["projects"][name]["md5"] = value["runs"][0]["md5"]
        index["projects"][name]["historical_same_environment_builds"] = len(value["runs"])
    gate_root = args.gate.resolve()
    gate = json.loads((gate_root / "gate.json").read_text())
    if not gate.get("complete") or gate["capture_sha256"] != index["capture_sha256"]:
        raise SystemExit("gate is incomplete or refers to a different capture")
    index["frozen_asset_gate"] = {"root": str(gate_root),
        "report": digest(gate_root / "gate.json"), "script": gate["script"],
        "policy": "Minimal child environments; frozen assets; converters forbidden"}
    for name, value in gate["projects"].items():
        if len(value["runs"]) != 2 or any(r["product"] != value["expected_rom"] for r in value["runs"]):
            raise SystemExit("missing or mismatching gate builds: " + name)
        index["projects"][name]["gate"] = {k: value[k] for k in
            ("generated_targets", "generated_assets", "inputs_manifest")}
        index["projects"][name]["gate"]["independent_matching_builds"] = 2
        index["projects"][name]["gate"]["environments"] = [r["environment"] for r in value["runs"]]
    index["nesrev"] = {}
    for name, modes in report["nesrev"].items():
        index["nesrev"][name] = {
            "source": digest(root / "inputs/nesrev/projects" / name / "asm" / (name + ".asm")),
            "modes": {mode: {"returncode": row["returncode"],
                              "binary": row["artifacts"]["binary.bin"]}
                      for mode, row in modes.items()},
        }
    if args.wire_review:
        evidence_root = args.wire_review.resolve()
        observations = json.loads((evidence_root / "observations.json").read_text())
        index["wire_followup"] = {"root": str(evidence_root),
            "report": digest(evidence_root / "observations.json"),
            "script": digest(evidence_root / "probe_ram_spans.py"),
            "span_cases": len(observations["spans"]),
            "ram_cases": len(observations["ram"]),
            "roms": len(observations["roms"]),
            "stable_sort_changed_roms": sum(bool(v.get("different_bytes"))
                                            for v in observations["roms"].values())}
    if args.sizeof_capture:
        evidence_root = args.sizeof_capture.resolve()
        observations = json.loads((evidence_root / "observations.json").read_text())
        index["sizeof_capture"] = {"root": str(evidence_root),
            "report": digest(evidence_root / "observations.json"),
            "script": digest(evidence_root / "probe_sizeof_capture.py"),
            "cases": len(observations["cases"]),
            "runs": sum(len(c["runs"]) for c in observations["cases"]),
            "fixed_label_scan": digest(evidence_root / "fixed-label-scan.json")}
    save(out, index)
    # Include original recorded inputs, not just their repository revisions.
    dest = local / "manifests"
    dest.mkdir(exist_ok=True)
    for source in sorted((root / "manifests").glob("*.json")):
        shutil.copy2(source, dest / source.name)

    # Freeze committed generated analysis fixtures and contracts as read-only
    # evidence. Their historical bundles need migration, not stale reuse.
    names = subprocess.check_output(["git", "ls-files", "-z"], cwd=args.nesrev).decode().split("\0")
    evidence = root / "consumer-evidence"
    already_frozen = evidence.exists()
    if not already_frozen:
        evidence.mkdir()
    selected = [n for n in names if n and
                (("/fixtures/" in n and "/tools/trace/" not in n and n.startswith("projects/"))
                 or ("/" not in n and (n.endswith("_SPEC.md") or n.endswith("_PLAN.md"))))]
    for name in ([] if already_frozen else selected):
        source = args.nesrev / name
        if source.is_file():
            target = evidence / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    evidence_manifest = manifest(evidence)
    saved_manifest = local / "manifests/consumer-evidence.json"
    if already_frozen and saved_manifest.exists() and json.loads(saved_manifest.read_text()) != evidence_manifest:
        raise SystemExit("frozen consumer evidence changed")
    save(saved_manifest, evidence_manifest)

    flags = ("listing", "listing-format", "listing-version", "xref", "xref-format",
             "xref-data", "xref-instructions", "xref-include-owner", "xref-include-locals",
             "xref-include-anon", "xref-summary", "analyze-index-patterns", "data-consumers",
             "analyze-data-coverage", "audit-raw-addresses", "compare", "dependency-manifest",
             "instruction-records-output", "fceux-nl-rom-prefix", "fceux-nl-ram-output")
    flag_hits = {"--" + flag: {} for flag in flags}
    readers = {}
    for source in sorted((root / "inputs/nesrev/scripts").rglob("*")):
        if not source.is_file() or source.suffix not in (".py", ".sh"):
            continue
        name = "scripts/" + str(source.relative_to(root / "inputs/nesrev/scripts"))
        text = source.read_text(errors="replace")
        for number, line in enumerate(text.splitlines(), 1):
            for flag, matches in flag_hits.items():
                if re.search(re.escape(flag) + r"(?=[^a-z-]|$)", line):
                    matches.setdefault(name, []).append(number)
        if re.search(r"json\.(load|loads)|read_json|\.load\(", text) and re.search(
                r"listing|xref|instruction_records|data_consumers|index_patterns|coverage|summary", text):
            readers[name] = digest(source)
    inventory = {"nesrev_commit": report["nesrev_commit"],
                 "producer_flag_hits": flag_hits, "reader_candidates": readers,
                 "note": "Static inventory plus D0's named-consumer checklist; not a claim of migrated consumers."}
    save(local / "consumer-inventory.json", inventory)
    print("indexed", len(index["projects"]), "ROM projects,",
          len(readers), "reader candidates,", len(selected), "fixture/contract files")


if __name__ == "__main__":
    main()
