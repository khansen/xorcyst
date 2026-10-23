#!/usr/bin/env python3
"""Syntactic PC-use rescan of frozen sources, with a planted positive control.

This is an inventory, not a parser or a proof about transitive aliases/macros.
"""
import argparse
import json
from pathlib import Path
import re


def scan(text):
    depth, hits = 0, []
    for number, line in enumerate(text.splitlines(), 1):
        code = re.sub(r'"[^"\n]*"|;.*$', "", line).strip()
        if re.match(r"(?:\w+\s+)?\.?(?:struc|union)\b", code, re.I):
            depth += 1
        elif re.match(r"\.?ends\b", code, re.I):
            depth = max(0, depth - 1)
        elif re.search(r"\$(?![0-9a-f])", code, re.I):
            if depth:
                hits.append({"line": number, "kind": "type-body", "text": line})
            if re.match(r"(?:\w+:\s*)?\.?(?:if|elif|rept|while|until|message|warning|error)\b", code, re.I):
                hits.append({"line": number, "kind": "expansion", "text": line})
    return hits


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("capture", type=Path)
    p.add_argument("--gate", type=Path, help="scan revised frozen game inputs")
    args = p.parse_args()
    positive = scan('.struc S\nfa .byte[$+1]\n.ends\n.if $ == 2\n.endif\n')
    if [h["kind"] for h in positive] != ["type-body", "expansion"]:
        raise AssertionError("positive control failed")
    if scan('.if $FA == 250 ; $\n.message "$"\n'):
        raise AssertionError("hex/string/comment control failed")
    results = {}
    roots = {p.name: p for p in (args.capture / "inputs").iterdir()}
    if args.gate:
        roots.update({p.name: p for p in (args.gate / "inputs").iterdir()})
    for root in sorted(roots.values()):
        if not root.is_dir():
            continue
        files = [f for f in root.rglob("*") if f.is_file() and f.suffix.lower() in (".asm", ".inc", ".h")]
        hits = [{"file": str(f.relative_to(root)), **h} for f in files
                for h in scan(f.read_text(errors="replace"))]
        results[root.name] = {"files": len(files), "hits": hits}
    dest = Path(__file__).resolve().parent / "pc-scan.json"
    dest.write_text(json.dumps({"positive_control": positive, "projects": results}, indent=2) + "\n")
    print(json.dumps({name: {"files": r["files"], "hits": len(r["hits"])} for name, r in results.items()}))


if __name__ == "__main__":
    main()
