#!/usr/bin/env python3
"""Bounded paired survey; no models or copied corpus files survive the run.

Compare matched release builds, and use a separately built test-only instrumented
producer to separate serialization from assembly/capture. It is not a formal
performance pass and does not measure downstream service gates or document work.
"""
import argparse
import gc
import json
import os
from pathlib import Path
import platform
import re
import resource
import statistics
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from source_model_support import reconstruct, sha, validate
from tool_test_support import build_tool


def timed(command, root, environment):
    start = time.perf_counter()
    with (root / "stdout").open("wb") as out, (root / "stderr").open("wb") as err:
        child = subprocess.Popen(command, cwd=root, env=environment, stdout=out, stderr=err)
        _, status, usage = os.wait4(child.pid, 0)
        child.returncode = os.waitstatus_to_exitcode(status)
    if child.returncode:
        raise ValueError((command, child.returncode, (root / "stderr").read_text()[-2000:]))
    return dict(elapsed_seconds=time.perf_counter() - start, cpu_seconds=usage.ru_utime + usage.ru_stime,
                maxrss=usage.ru_maxrss, maxrss_unit="bytes" if sys.platform == "darwin" else "KiB")


def consumer(model_path, result_path):
    start = time.perf_counter()
    model = json.loads(model_path.read_bytes())
    decoded = time.perf_counter() - start
    start = time.perf_counter()
    validate(model)
    checked = time.perf_counter() - start
    start = time.perf_counter()
    reconstruct(model)
    rebuilt = time.perf_counter() - start
    result_path.write_text(json.dumps(dict(decode_seconds=decoded, validation_including_reconstruction_seconds=checked,
                                         reconstruction_seconds=rebuilt, peak_rss=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--consumer", type=Path)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--baseline", type=Path)
    p.add_argument("--candidate", type=Path)
    p.add_argument("--inputs", type=Path)
    p.add_argument("--name", action="append", default=[])
    p.add_argument("--pairs", type=int, default=7)
    args = p.parse_args()
    if args.consumer:
        consumer(args.consumer, args.output)
        return
    if args.pairs < 1:
        p.error("pairs must be positive")
    tools = {"baseline": args.baseline.resolve(), "candidate": args.candidate.resolve()}
    report = dict(kind="bounded paired survey", host=platform.platform(), pairs=args.pairs,
                  tools={k: {"path": str(v), "sha256": sha(v.read_bytes())} for k, v in tools.items()}, results=[])
    with tempfile.TemporaryDirectory(prefix="xasm-source-model-measure-") as directory:
        root = Path(directory)
        instrumented = root / "instrumented-xasm"
        os.environ["TEST_SOURCE_MODEL_CFLAGS"] = "-std=gnu23 -g -O2"
        build_tool("xasm", instrumented, "TEST_SOURCE_MODEL_CFLAGS", {"source_model.c": "tests/test_source_model_faults.c"})
        report["instrumented_sha256"] = sha(instrumented.read_bytes())
        inputs = json.loads(args.inputs.read_bytes())
        inputs = [i for i in inputs if not args.name or i["name"] in args.name]
        inputs.insert(0, dict(name="neutral-small", source_bytes=".ORG $8000\nCell .EQU $10\nEntry: LDA Cell\nSTA Cell,X\nBNE Entry\n.DW Entry\n"))
        environment = dict(PATH="/usr/bin:/bin", HOME=str(root), TMPDIR=str(root), LC_ALL="C", LANG="C", TZ="UTC")
        report["environment"] = environment
        for item in inputs:
            source = item["source_bytes"].encode() if "source_bytes" in item else Path(item["source"]).read_bytes()
            if "source_sha256" in item and sha(source) != item["source_sha256"]:
                raise ValueError("INPUT_CHANGED")
            (root / "input.asm").write_bytes(source)
            rows = {k: [] for k in ("baseline_plain", "candidate_plain", "baseline_records", "candidate_records", "model", "instrumented", "consumer")}
            expected = item.get("prg_sha256")
            for pair in range(args.pairs):
                for mode in ("plain", "records"):
                    for name in (["baseline", "candidate"] if pair % 2 == 0 else ["candidate", "baseline"]):
                        options = [] if mode == "plain" else ["--instruction-records-output=records.json", "--dependency-manifest=deps.json"]
                        rows[name + "_" + mode].append(timed([str(tools[name]), "--pure-binary", "input.asm", "-o", "out", *options], root, environment))
                        actual = sha((root / "out").read_bytes())
                        expected = expected or actual
                        if actual != expected:
                            raise ValueError("PRG_MISMATCH")
                for name, tool, env in (("model", tools["candidate"], environment),
                                        ("instrumented", instrumented, {**environment, "XASM_TEST_MODEL_MEASURE": "1"})):
                    result = timed([str(tool), "--pure-binary", "input.asm", "-o", "out", "--source-model=model.json"], root, env)
                    if sha((root / "out").read_bytes()) != expected:
                        raise ValueError("PRG_MISMATCH")
                    if name == "instrumented":
                        match = re.search(r"SOURCE_MODEL_MEASURE ([0-9.]+) ([0-9.]+)", (root / "stderr").read_text())
                        result["serialization_seconds"] = float(match[1])
                        result["serialization_cpu_seconds"] = float(match[2])
                        result["capture_assembly_seconds"] = result["elapsed_seconds"] - float(match[1])
                    rows[name].append(result)
                rows["consumer"].append(timed([sys.executable, "-B", str(Path(__file__).resolve()), "--consumer", str(root / "model.json"),
                                                "--output", str(root / "consumer.json")], root, environment))
                rows["consumer"][-1].update(json.loads((root / "consumer.json").read_bytes()))
            report["results"].append(dict(name=item["name"], source_bytes=len(source), source_sha256=sha(source), prg_sha256=expected,
                                         artifact_bytes=(root / "model.json").stat().st_size,
                                         retained_bytes=0, temporary_bytes=sum(f.stat().st_size for f in root.iterdir()),
                                         samples=rows, medians={k: statistics.median(r["elapsed_seconds"] for r in v) for k, v in rows.items()}))
            args.output.write_text(json.dumps(report, indent=2) + "\n")
            (root / "model.json").unlink()
            print(item["name"], report["results"][-1]["medians"], flush=True)
            gc.collect()
    report["complete"] = True
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
