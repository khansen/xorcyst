#!/usr/bin/env python3
"""Freeze owner-local D1 inputs and rebuild v1.9.12 in isolated directories.

This captures source/converter provenance, not a reproducible XORcyst gate.
Use freeze_gate_assets.py for verified frozen-asset builds: converter outputs
(specially Wishmaster xm2nes) can depend on the environment.
This is evidence tooling, not a v2 implementation or a performance benchmark.
The destination must be new. Original project directories are only read.
Private project sources/ROMs stay outside the committed contract directory.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time


MUSIC = (
    "bob-omb-battlefield", "dire-dire-docks", "double-dragon-ii-vs-f1",
    "dpadhero3proto", "is-it-scary", "live-and-learn", "mega-jean-trooper",
    "merry-xmas-everybody", "somewhere-only-we-know", "there-must-be-an-angel",
    "welcome-to-paradise", "wishmaster",
)
PROFILES = (
    "ci-data-v1", "ci-instructions-v1", "inventory-instructions-v1",
    "process-instructions-v1", "maturity-instructions-v1", "instructions-v1",
    "data-listing-v1", "pass-prep-instructions-v1",
)


def minimal_environment(home, path):
    """Whitelist only; in particular never inherit fault hooks or MAKEFLAGS."""
    return {"HOME": str(home), "TMPDIR": str(home), "PATH": path,
            "LC_ALL": "C", "LANG": "C", "TZ": "UTC",
            "PYTHONDONTWRITEBYTECODE": "1"}


def digest(path):
    data = Path(path).read_bytes()
    return {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def manifest(root):
    return {str(p.relative_to(root)): digest(p) for p in sorted(root.rglob("*"))
            if p.is_file() and "__pycache__" not in p.parts}


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def ignored(directory, names):
    return {name for name in names if name in {".git", ".DS_Store", "__pycache__"}
            or name.endswith((".o", ".nes", ".pyc"))}


def snapshot(source, target):
    # Dereference shared-source links so the frozen copy needs no live tree.
    # Record the resolved files individually, including working-tree edits.
    shutil.copytree(source, target, ignore=ignored)
    return manifest(target)


def run(argv, cwd, logfile, env=None):
    logfile.parent.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    with logfile.open("wb") as log:
        result = subprocess.run(argv, cwd=cwd, env=env, stdout=log,
                                stderr=subprocess.STDOUT, check=False)
    row = {"argv": argv, "cwd": str(cwd), "returncode": result.returncode,
           "elapsed_seconds": time.monotonic() - started, "log": str(logfile)}
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {argv}; see {logfile}")
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--github", type=Path, default=Path.home() / "dev/github")
    parser.add_argument("--ntxdev", type=Path, default=Path.home() / "dev/ntxdev")
    args = parser.parse_args()
    root = args.destination.absolute()
    root.mkdir(parents=True, exist_ok=False)
    (root / ".gitignore").write_text("*\n")
    inputs, builds, logs = (root / n for n in ("inputs", "builds", "logs"))
    inputs.mkdir()
    builds.mkdir()
    logs.mkdir()
    repo = Path(__file__).resolve().parents[2]
    state = {"baseline": "v1.9.12", "capture_script": digest(__file__),
             "platform": platform.platform(), "machine": platform.machine(),
             "python": sys.version, "projects": {}, "tools": {},
             "commands": [], "complete": False}
    save(root / "capture.json", state)
    archive = root / "xorcyst-v1.9.12.tar"
    with archive.open("wb") as out:
        subprocess.run(["git", "archive", "v1.9.12"], cwd=repo, stdout=out, check=True)
    state["baseline_commit"] = subprocess.check_output(
        ["git", "rev-parse", "v1.9.12^{commit}"], cwd=repo, text=True).strip()
    state["source_archive"] = digest(archive)
    baseline = builds / "v1.9.12"
    baseline.mkdir()
    subprocess.run(["tar", "-xf", str(archive), "-C", str(baseline)], check=True)
    env = minimal_environment(root, "/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/opt/homebrew/bin")
    state["compiler"] = subprocess.check_output(["cc", "--version"], text=True, env=env)
    state["commands"].append(run(["./configure", "--disable-maintainer-mode",
        "YACC=:", "LEX=:", "HELP2MAN=:", "CC=cc", "CFLAGS=-O2 -g -Werror"],
        baseline, logs / "configure.log", env))
    state["commands"].append(run(["make", "-j2"], baseline, logs / "build.log", env))
    toolbin = root / "tools"
    toolbin.mkdir()
    for name in ("xasm", "xlnk", "xm2nes", "xm2btn", "packchr", "packnam"):
        source = baseline / name if name in ("xasm", "xlnk") else Path(shutil.which(name))
        shutil.copy2(source, toolbin / name)
        state["tools"][name] = {"original": str(source), **digest(source)}
        if platform.system() == "Darwin":
            state["tools"][name]["dynamic_libraries"] = subprocess.check_output(
                ["otool", "-L", str(source)], text=True)
    # The game's Makefile selects this executable rather than the PATH version.
    source = args.github / "dpadhero2/tools/xm2btn/xm2btn"
    shutil.copy2(source, toolbin / "dpadhero2-xm2btn")
    state["tools"]["dpadhero2-xm2btn"] = {"original": str(source), **digest(source)}
    env["PATH"] = str(toolbin) + os.pathsep + env["PATH"]
    env["XASM_BIN"] = str(toolbin / "xasm")
    definitions = [
        ("bombsweeper", args.github / "bombsweeper", ".", "bombswpr.nes", "bombswpr.nes"),
        ("dpadhero2", args.github / "dpadhero2", "src", "build/dpadhero2.nes", "build/dpadhero2.nes"),
        ("ntxdev", args.ntxdev, ".", "ntxdev.nes", "ntxdev.nes"),
    ]
    for name in MUSIC:
        source = args.github / "nesdev/music" / name
        program = re.search(r"^PROGRAM\s*=\s*(\S+)",
                            (source / "Makefile").read_text(), re.M).group(1)
        product = re.search(r"output\s*\{\s*file\s*=\s*([^}\s]+)",
                            (source / "link.s").read_text()).group(1)
        definitions.append(("music-" + name, source, ".", program + ".nes", product))
    for name, source, subdir, target, product_name in definitions:
        print("Freezing and building", name, flush=True)
        files = snapshot(source, inputs / name)
        save(root / "manifests" / (name + ".json"), files)
        row = {"source": str(source), "build_directory": subdir, "target": target,
               "product": product_name,
               "manifest_sha256": digest(root / "manifests" / (name + ".json"))["sha256"],
               "files": len(files), "runs": []}
        state["projects"][name] = row
        for repeat in (1, 2):
            build = builds / (name + f"-{repeat}")
            shutil.copytree(inputs / name, build)
            xm2btn = "dpadhero2-xm2btn" if name == "dpadhero2" else "xm2btn"
            argv = ["make", "-B", "-j1", target,
                    "XASM=" + str(toolbin / "xasm") + " $(AFLAGS)",
                    "XLNK=" + str(toolbin / "xlnk") + " $(LFLAGS)",
                    "XM2NES=" + str(toolbin / "xm2nes"),
                    "XM2BTN=" + str(toolbin / xm2btn)]
            record = run(argv, build / subdir, logs / f"{name}-{repeat}.log", env)
            product = build / subdir / product_name
            record["product"] = digest(product)
            record["md5"] = hashlib.md5(product.read_bytes()).hexdigest()
            row["runs"].append(record)
            save(root / "capture.json", state)
        if row["runs"][0]["product"] != row["runs"][1]["product"]:
            raise RuntimeError(f"non-reproducible ROM: {name}")
        print(name, row["runs"][0]["product"], flush=True)
    # Freeze the actual consumer, its profile definitions and the two large
    # source files. Keep this snapshot local: it includes project source/assets.
    nesrev = inputs / "nesrev"
    nesrev.mkdir()
    snapshot(args.github / "nesrev/scripts", nesrev / "scripts")
    for name in ("ANALYSIS_BUNDLE_SPEC.md", "NESREV_STRUCTURED_ANALYSIS_MIGRATION_PLAN.md"):
        shutil.copy2(args.github / "nesrev" / name, nesrev / name)
    for name in ("metroid", "zelda"):
        source = args.github / "nesrev/projects" / name
        dest = nesrev / "projects" / name
        dest.mkdir(parents=True)
        snapshot(source / "asm", dest / "asm")
        snapshot(source / "config", dest / "config")
        shutil.copy2(source / "project.conf", dest / "project.conf")
        (dest / "reference").mkdir()
        shutil.copy2(source / "reference" / (name + ".nes"),
                     dest / "reference" / (name + ".nes"))
    save(root / "manifests/nesrev.json", manifest(nesrev))
    script = nesrev / "scripts/analysis_bundle.py"
    state["nesrev_commit"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=args.github / "nesrev", text=True).strip()
    state["nesrev"] = {}
    for name in ("metroid", "zelda"):
        source = nesrev / "projects" / name / "asm" / (name + ".asm")
        config = nesrev / "projects" / name / "project.conf"
        rows = state["nesrev"][name] = {}
        for profile in PROFILES:
            output = root / "analysis" / name / profile
            output.mkdir(parents=True)
            if profile in ("instructions-v1", "data-listing-v1"):
                prepare = [sys.executable, str(script), "prepare-source", str(output),
                           str(source), "--profile", profile]
            else:
                prepare = [sys.executable, str(script), "prepare", str(output), name,
                           str(config), digest(config)["sha256"], str(source), "$C000-$FFFF", "$C000",
                           "--profile", profile]
            run(prepare, nesrev, logs / f"{name}-{profile}-prepare.log", env)
            rows[profile] = run([sys.executable, str(script), "produce", str(output),
                str(source), str(output / "binary.bin"), "--profile", profile],
                nesrev, logs / f"{name}-{profile}.log", env)
            rows[profile]["artifacts"] = manifest(output)
            save(root / "capture.json", state)
            print(name, profile, "validated", flush=True)
        hashes = {r["artifacts"]["binary.bin"]["sha256"] for r in rows.values()}
        if len(hashes) != 1:
            raise RuntimeError(f"analysis profiles changed binary: {name}")
        reference = nesrev / "projects" / name / "reference" / (name + ".nes")
        binary = (root / "analysis" / name / PROFILES[0] / "binary.bin").read_bytes()
        if reference.read_bytes()[16:16 + len(binary)] != binary:
            raise RuntimeError(f"binary differs from reference PRG: {name}")
        reference_prg = root / (name + "-reference-prg.bin")
        reference_prg.write_bytes(reference.read_bytes()[16:16 + len(binary)])
        # Independent producer modes, as required by the no-slowdown contract.
        # These single runs record correctness artifacts, not timing acceptance.
        modes = {
            "plain": [],
            "text-listing": ["--listing={out}/listing.txt"],
            "json-v2": ["--listing={out}/listing.json", "--listing-format=json", "--listing-version=2"],
            "ndjson-v2": ["--listing={out}/listing.ndjson", "--listing-format=ndjson", "--listing-version=2"],
            "xref": ["--xref={out}/xref.json"],
            "xref-data-owner": ["--xref={out}/xref.json", "--xref-data=true", "--xref-include-owner=true"],
            "xref-records": ["--xref={out}/xref.json", "--xref-instructions=true"],
            "records": ["--instruction-records-output={out}/records.json", "--dependency-manifest={out}/deps.json"],
            "summary": ["--xref-summary", "--xref-summary-output={out}/summary.json"],
            "index-patterns": ["--analyze-index-patterns", "--index-patterns-output={out}/index.json"],
            "data-consumers": ["--data-consumers", "--data-consumers-output={out}/consumers.json"],
            "coverage": ["--analyze-data-coverage", "--data-coverage-output={out}/coverage.json"],
            "compare": ["--compare=" + str(reference_prg), "--compare-format=json", "--compare-cpu-base=$C000"],
            "audit": ["--audit-raw-addresses", "--audit-rom-range=$C000-$FFFF", "--audit-output-format=json"],
            "fceux": ["--fceux-nl-rom-prefix={out}/rom", "--fceux-nl-ram-output={out}/ram.nl"],
        }
        for mode, flags in modes.items():
            output = root / "analysis" / name / mode
            output.mkdir()
            argv = [str(toolbin / "xasm"), "--pure-binary", str(source), "-o", str(output / "binary.bin")]
            argv += [flag.format(out=output) for flag in flags]
            rows[mode] = run(argv, nesrev, logs / f"{name}-{mode}.log", env)
            rows[mode]["artifacts"] = manifest(output)
            if (output / "binary.bin").read_bytes() != binary:
                raise RuntimeError(f"producer mode changed binary: {name}/{mode}")
            save(root / "capture.json", state)
        print(name, "15 independent modes match reference PRG", flush=True)
    # The frozen inputs must not have changed as a side effect of builds.
    for name, source, _, _, _ in definitions:
        expected = json.loads((root / "manifests" / (name + ".json")).read_text())
        if manifest(inputs / name) != expected:
            raise RuntimeError(f"frozen inputs changed: {name}")
        # Check all included files against their original working-tree bytes.
        if any(digest(source / rel) != value for rel, value in expected.items()):
            raise RuntimeError(f"original changed during capture: {name}")
    state["complete"] = True
    shutil.copy2(__file__, root / "capture-script.py")
    state["nesrev_manifest_sha256"] = digest(root / "manifests/nesrev.json")["sha256"]
    save(root / "capture.json", state)
    print("Complete:", root / "capture.json", flush=True)


if __name__ == "__main__":
    main()
