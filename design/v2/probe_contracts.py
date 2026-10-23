#!/usr/bin/env python3
"""Capture v1 observations for D1 examples; no candidate implementation is used."""
import argparse
import itertools
import json
from pathlib import Path
import subprocess
from freeze_baselines import minimal_environment


CASES = [
    {"id": "first-walk-263", "source": "K .equ 263\nBack: nop\nlda Back\nlda K-L\nnop\nL: rts\n",
     "v2": "EA A5 00 AD 00 01 EA 60"},
    {"id": "first-walk-262", "source": "K .equ 262\nBack: nop\nlda Back\nlda K-L\nnop\nL: rts\n",
     "v2": "error"},
    {"id": "cascade", "origin": 0xF6, "source": "lda L\nlda M\nM: nop\nnop\nnop\nnop\nL: rts\n",
     "v2": "A5 FE A5 FA EA EA EA EA 60"},
    {"id": "no-trial-rescue", "origin": 0xFA, "source": "M .equ $10\nlda M\nlda L\nnop\nL: rts\n",
     "v2": "A5 10 AD 00 01 EA 60"},
    {"id": "negative-shortened", "source": "lda T-3\nT: nop\n", "v2": "error"},
    {"id": "signed-wide", "source": "Z .equ $10\nlda Z\nlda T-6\nT: nop\n",
     "v2": "A5 10 AD FF FF EA"},
    {"id": "late-wide-range", "source": "jmp L+$10000\nL: rts\n",
     "v2": {"binary": "4C 03 00 60 with warning", "object": "error"}},
    {"id": "independent-code-extent", "source": ".dsb Tail-Head\nHead: nop\nnop\nTail: rts\n",
     "v2": "00 00 EA EA 60"},
    {"id": "forward-string", "source": '.byte Text\nAfter: .word After\nText .equ "ABC"\n',
     "v2": "41 42 43 03 00"},
    {"id": "forward-string-plus-zero", "source": '.byte S+0\nS .equ "A"\n', "v2": "41 30"},
    {"id": "known-string-plus-zero", "source": 'S .equ "A"\n.byte S+0\n', "v2": "41 30"},
    {"id": "forward-char-map", "source": '.charmap "map.txt"\n.char Text,"HI"\nText .equ "HI"\n',
     "v2": "01 00 01 00"},
    {"id": "field-instance-pc", "source": '.struc I\nfa .byte\nfb .byte\n.ends\n.struc S\nitems .I[3-$]\ntl .byte\n.ends\n.S {}\nAfter: .word After\n',
     "v2": "error"},
    {"id": "forward-assignment", "source": ".byte I\nI = 1\nI = 2\n", "v2": "02"},
    {"id": "undef-forward", "source": ".byte Q\n.define Q 1\n.undef Q\n.define Q 2\n", "v2": "02"},
    {"id": "provisional-pc-expansion", "source": "lda Fwd\n.if $ == 3\n.byte 3\n.else\n.byte 2\n.endif\nFwd .equ $10\n",
     "v2": "error"},
    {"id": "absolute-pc-expansion", "source": "nop\n.if $ == 1\n.byte 1\n.endif\n",
     "v2": {"binary": "EA 01", "object": "error"}},
    {"id": "data-label-extent", "source": ".dataseg\nFirst .byte\nLast .byte[First+1]\n.codeseg\n.byte 9\n",
     "v2": {"binary": "09", "object": "error"}},
    {"id": "type-capture", "source": "N=2\n.struc S\nf .byte[N]\n.ends\n.dsb sizeof S\nN=3\n.byte sizeof S,$\n",
     "v2": "00 00 03 03"},
    {"id": "fixed-ram-loop", "origin": 0x8000,
     "source": ".dataseg\n.zeropage v .byte\n.codeseg\nloop:\n" + "lda v\n" * 43 + "bne loop\n",
     "binary_source": ".dataseg\n.org $10\n.zeropage v .byte\n.codeseg\nloop:\n" + "lda v\n" * 43 + "bne loop\n",
     "v2": "A5 10 repeated 43 times, then D0 A8"},
    {"id": "exported-forward-alias", "units": {
         "consumer": ".extrn Alias : byte\n.byte Alias\n",
         "provider": ".public Alias\nAlias .equ Value\nValue .equ 7\n"},
     "v2": "07 (either link order)"},
    {"id": "imported-char-map", "units": {
         "consumer": '.extrn Text : byte\n.charmap "map.txt"\n.char Text,"HI"\n',
         "provider": '.public Text\nText .equ "HI"\n'}, "v2": "01 00 01 00 (either link order)"},
    {"id": "exported-import-alias", "units": {
         "consumer": ".extrn Alias : byte\n.byte Alias\n",
         "alias": ".public Alias\n.extrn Value : byte\nAlias .equ Value+1\n",
         "provider": ".public Value\nValue .equ 7\n"}, "v2": "08 (all six link orders)"},
    {"id": "zero-field-remainder", "units": {
         "consumer": '.extrn N : byte\n.struc S\nnm .char[N]\nfb .byte\n.ends\nv .S {"AB",7}\n.byte $FF\n',
         "provider": '.public N\nN .equ 2\n'}, "v2": "41 42 07 FF (either link order)"},
    {"id": "implicit-external", "units": {
         "consumer": "jsr reset\n", "provider": ".public reset\nreset: rts\n"},
     "v2": {"consumer-first": "20 03 00 60", "provider-first": "60 20 00 00"}},
]


def invoke(argv, cwd):
    p = subprocess.run([str(a) for a in argv], cwd=cwd, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, timeout=15,
                       env=minimal_environment(cwd, "/usr/bin:/bin"))
    return {"argv": [str(a) for a in argv], "status": p.returncode,
            "stdout": p.stdout.decode(errors="replace"),
            "stderr": p.stderr.decode(errors="replace")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tools", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    root = args.destination.resolve()
    root.mkdir(parents=True, exist_ok=False)
    tools = args.tools.resolve()
    report = []
    for case in CASES:
        work = root / case["id"]
        work.mkdir()
        (work / "map.txt").write_text("H=1\nI=0\n")
        row = {**case, "observed_v1": {}}
        origin = case.get("origin", 0)
        if "source" in case:
            (work / "binary.asm").write_text(f".org ${origin:X}\n" + case.get("binary_source", case["source"]))
            output = work / "binary.bin"
            output.write_bytes(b"old output")
            result = invoke([tools / "xasm", "--pure-binary", "binary.asm", "-o", output.name], work)
            result["bytes_or_preserved"] = output.read_bytes().hex(" ").upper()
            if result["status"] and output.read_bytes() != b"old output":
                raise AssertionError("failed binary overwrote output")
            row["observed_v1"]["binary"] = result
            units = {"main": case["source"]}
        else:
            units = case["units"]
        for name, source in units.items():
            (work / (name + ".asm")).write_text(source)
            result = invoke([tools / "xasm", name + ".asm", "-o", name + ".o"], work)
            row["observed_v1"]["assemble-" + name] = result
        if all(row["observed_v1"]["assemble-" + n]["status"] == 0 for n in units):
            for order in itertools.permutations(units):
                script = "ram{start=$10,end=$7FF}\n" + "\n".join(
                    f"link{{file={n}.o" + (f",origin=${origin:X}" if i == 0 else "") + "}"
                    for i, n in enumerate(order)) + "\n"
                (work / "link.s").write_text(script)
                output = work / "linked.bin"
                output.write_bytes(b"old output")
                result = invoke([tools / "xlnk", "link.s", "-o", output.name], work)
                result["bytes_or_preserved"] = output.read_bytes().hex(" ").upper()
                if result["status"] and output.read_bytes() != b"old output":
                    raise AssertionError("failed link overwrote output")
                row["observed_v1"]["link-" + "-".join(order)] = result
        report.append(row)
        if any("syntax error" in r["stderr"] for r in row["observed_v1"].values()):
            raise AssertionError("invalid probe syntax: " + case["id"])
    (root / "observations.json").write_text(json.dumps(report, indent=2) + "\n")
    for row in report:
        print(row["id"], [(n, r["status"], r.get("bytes_or_preserved", ""))
                          for n, r in row["observed_v1"].items()
                          if n == "binary" or n.startswith("link-")])


if __name__ == "__main__":
    main()
