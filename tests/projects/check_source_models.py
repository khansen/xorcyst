#!/usr/bin/env python3
"""Optional private corpus sweep; retains hashes/counts, never source models.

The input JSON is a list of {name, source, source_sha256, prg_sha256}. Assemble
each source once, check the independently pinned bytes, delete the copied input
and binary, then validate and reconstruct from the saved model alone.
"""
import argparse
import gc
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from source_model_support import sha, validate


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("xasm", type=Path)
    p.add_argument("inputs", type=Path)
    p.add_argument("output", type=Path)
    args = p.parse_args()
    tool = args.xasm.resolve()
    report = {"producer_sha256": sha(tool.read_bytes()), "projects": [], "complete": False}
    for item in json.loads(args.inputs.read_bytes()):
        source = Path(item["source"]).read_bytes()
        if sha(source) != item["source_sha256"]:
            raise ValueError("CORPUS_INPUT_CHANGED")
        with tempfile.TemporaryDirectory(prefix="xasm-model-corpus-") as directory:
            root = Path(directory)
            (root / "input.asm").write_bytes(source)
            environment = dict(PATH="/usr/bin:/bin", HOME=str(root), TMPDIR=str(root), LC_ALL="C", LANG="C")
            command = [str(tool), "--pure-binary", "input.asm", "-o", "output.bin", "--source-model=model.json"]
            start = time.perf_counter()
            with (root / "stdout").open("wb") as out, (root / "stderr").open("wb") as err:
                process = subprocess.Popen(command, cwd=root, env=environment, stdout=out, stderr=err)
                _, status, usage = os.wait4(process.pid, 0)
                process.returncode = os.waitstatus_to_exitcode(status)
            elapsed = time.perf_counter() - start
            if process.returncode:
                raise ValueError((item["name"], process.returncode, (root / "stderr").read_text()[-4000:]))
            if sha((root / "output.bin").read_bytes()) != item["prg_sha256"]:
                raise ValueError("CORPUS_PRG_MISMATCH")
            temporary_bytes = sum(f.stat().st_size for f in root.iterdir())
            artifact_bytes = (root / "model.json").stat().st_size
            (root / "input.asm").unlink()
            (root / "output.bin").unlink()
            start = time.perf_counter()
            model = json.loads((root / "model.json").read_bytes())
            decoded = time.perf_counter() - start
            start = time.perf_counter()
            reconstructed = validate(model)
            checked = time.perf_counter() - start
            if reconstructed != {"input.asm": source}:
                raise ValueError("CORPUS_ROUNDTRIP_MISMATCH")
            row = dict(item, source_bytes=len(source), artifact_bytes=artifact_bytes,
                       temporary_bytes=temporary_bytes, assembly_export_seconds=elapsed,
                       producer_cpu_seconds=usage.ru_utime + usage.ru_stime,
                       producer_maxrss=usage.ru_maxrss, maxrss_unit="bytes" if sys.platform == "darwin" else "KiB",
                       decode_seconds=decoded, validation_reconstruction_seconds=checked,
                       source_roundtrip=True, prg_parity=True,
                       counts={k: len(model[k]) for k in ("units", "syntax", "declarations", "references", "expressions", "instructions", "data", "segments", "emissions")})
            report["projects"].append(row)
            args.output.write_text(json.dumps(report, indent=2) + "\n")
            print(item["name"], "PASS", row["counts"]["references"], "references", flush=True)
            del model, reconstructed
            gc.collect()
    report["complete"] = True
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
