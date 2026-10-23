#!/usr/bin/env python3
"""Freeze captured generated assets, then verify clean builds in two environments.

Only copies are built. The original capture is read-only, including its historical
logs. Converter stubs fail closed. Future candidate comparisons use these same
fixture inputs, never fresh converter output or v1 object files.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

from freeze_baselines import digest, ignored, manifest, minimal_environment, run, save

CONVERTERS = ("xm2nes", "xm2btn", "dpadhero2-xm2btn", "packchr", "packnam")


def generated_targets(capture, name):
    """Read the actual capture commands; reject unknown converter recipes."""
    targets = set()
    for line in (capture / "logs" / (name + "-1.log")).read_text().splitlines():
        if not re.search(r"(?:^|/)(?:xm2nes|(?:dpadhero2-)?xm2btn|packchr|packnam) ", line):
            continue
        output = re.search(r">\s*(\S+)|--output=(\S+)", line)
        if output:
            targets.add(output[1] or output[2])
        elif line.startswith("packchr "):
            targets.update(("packchr.chr", "packchr.nam"))
        elif line.startswith("packnam "):
            targets.add("packnam.dat")
        else:
            raise ValueError("unknown converter command: " + line)
    for target in targets:
        if Path(target).is_absolute() or ".." in Path(target).parts:
            raise ValueError("unsafe generated target: " + target)
    return sorted(targets)


def recover_dpadhero2(capture, root, project):
    # GNU make deleted all 23 generated .inc intermediates in the old capture.
    # Recover only this stable converter workload, preserving intermediates, and
    # require both generated files and complete ROMs to match across environments.
    outputs = []
    for repeat in (1, 2):
        context = root / "asset-generation" / ("a" if repeat == 1 else "longer-b")
        home, tools, source = (context / n for n in ("home", "tools", "source"))
        home.mkdir(parents=True)
        tools.mkdir()
        for tool in ("xasm", "xlnk", *CONVERTERS):
            shutil.copy2(capture / "tools" / tool, tools / tool)
        shutil.copytree(capture / "inputs/dpadhero2", source)
        keep = context / "keep.mk"
        keep.write_text(".SECONDARY:\n")
        env = minimal_environment(home, str(tools) + ":/usr/bin:/bin:/usr/sbin:/sbin")
        env["LANG"] = "C" if repeat == 1 else "POSIX"
        argv = ["/usr/bin/make", "-f", "Makefile", "-f", str(keep), "-B", "-j1",
                project["target"], "XASM=" + str(tools / "xasm") + " $(AFLAGS)",
                "XLNK=" + str(tools / "xlnk") + " $(LFLAGS)",
                "XM2NES=" + str(tools / "xm2nes"),
                "XM2BTN=" + str(tools / "dpadhero2-xm2btn")]
        record = run(argv, source / "src", root / "logs" / f"dpadhero2-assets-{repeat}.log", env)
        record["environment"] = env
        if digest(source / "src" / project["product"]) != project["runs"][0]["product"]:
            raise ValueError("dpadhero2 recovered assets changed reference ROM")
        save(context / "generation.json", record)
        outputs.append(source)
    return outputs


def verify(root, capture, report):
    for name, project in report["projects"].items():
        inputs = root / "inputs" / name
        expected = json.loads((root / "manifests" / (name + ".json")).read_text())
        if manifest(inputs) != expected:
            raise ValueError("changed frozen inputs: " + name)
        for repeat in (1, 2):
            # Different HOME, TMPDIR, PATH/tool path lengths and cwd, but the
            # same executable bytes. Neither environment inherits the shell.
            label = "a" if repeat == 1 else "independent-longer-environment-b"
            context = root / "builds" / label / name
            home, tools, build = (context / n for n in ("home", "tools", "source"))
            home.mkdir(parents=True)
            tools.mkdir()
            for tool in ("xasm", "xlnk"):
                shutil.copy2(capture / "tools" / tool, tools / tool)
            for tool in CONVERTERS:
                stub = tools / tool
                stub.write_text('#!/bin/sh\necho "converter forbidden in XORcyst gate" >&2\nexit 99\n')
                stub.chmod(0o755)
            shutil.copytree(inputs, build)
            env = minimal_environment(home, str(tools) + ":/usr/bin:/bin:/usr/sbin:/sbin")
            # Also changes a harmless whitelisted variable, to verify that the
            # second run is not just a second invocation in the same context.
            env["LANG"] = "C" if repeat == 1 else "POSIX"
            argv = ["/usr/bin/make", "-B", "-j1"]
            for target in project["generated_targets"]:
                argv += ["-o", target]
            argv += [project["target"],
                     "XASM=" + str(tools / "xasm") + " $(AFLAGS)",
                     "XLNK=" + str(tools / "xlnk") + " $(LFLAGS)",
                     "XM2NES=" + str(tools / "xm2nes"),
                     "XM2BTN=" + str(tools / "xm2btn")]
            record = run(argv, build / project["build_directory"],
                         root / "logs" / f"{name}-{repeat}.log", env)
            record["environment"] = env
            product = build / project["build_directory"] / project["product"]
            record["product"] = digest(product)
            record["md5"] = hashlib.md5(product.read_bytes()).hexdigest()
            if record["product"] != project["expected_rom"]:
                raise ValueError("ROM differs from captured reference: " + name)
            # Make may create objects/ROMs but must not alter any fixture input.
            if any(digest(build / rel) != value for rel, value in expected.items()):
                raise ValueError("build changed fixture input: " + name)
            project["runs"].append(record)
            save(root / "gate.json", report)
        if manifest(inputs) != expected:
            raise ValueError("verification changed frozen inputs: " + name)
        print(name, project["runs"][0]["md5"], "matches in both environments", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    capture, root = args.capture.resolve(), args.destination.resolve()
    original = json.loads((capture / "capture.json").read_text())
    if not original.get("complete"):
        raise ValueError("incomplete historical capture")
    root.mkdir(parents=True, exist_ok=False)
    report = {"complete": False, "capture_root": str(capture),
              "capture_sha256": digest(capture / "capture.json")["sha256"],
              "script": digest(__file__), "tools": {}, "projects": {}}
    for tool in ("xasm", "xlnk"):
        value = digest(capture / "tools" / tool)
        if value["sha256"] != original["tools"][tool]["sha256"]:
            raise ValueError("changed tool: " + tool)
        report["tools"][tool] = value
    recovered = recover_dpadhero2(capture, root, original["projects"]["dpadhero2"])
    for name, project in original["projects"].items():
        source = capture / "inputs" / name
        expected = json.loads((capture / "manifests" / (name + ".json")).read_text())
        if manifest(source) != expected:
            raise ValueError("changed captured source: " + name)
        inputs = root / "inputs" / name
        shutil.copytree(source, inputs, ignore=ignored)
        targets = generated_targets(capture, name)
        assets = {}
        for target in targets:
            rel = Path(project["build_directory"]) / target
            a, b = ([source / rel for source in recovered] if name == "dpadhero2" else
                    [capture / "builds" / f"{name}-{i}" / rel for i in (1, 2)])
            value = digest(a)
            if value != digest(b):
                raise ValueError("capture asset copies disagree: " + name + "/" + target)
            dest = inputs / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(a, dest)
            assets[str(rel)] = value
        files = manifest(inputs)
        if any(p.endswith((".o", ".nes")) for p in files):
            raise ValueError("precompiled product in fixture: " + name)
        save(root / "manifests" / (name + ".json"), files)
        report["projects"][name] = {
            **{k: project[k] for k in ("build_directory", "target", "product")},
            "generated_targets": targets, "generated_assets": assets,
            "inputs_manifest": digest(root / "manifests" / (name + ".json")),
            "original_manifest_sha256": project["manifest_sha256"],
            "expected_rom": project["runs"][0]["product"], "runs": []}
    save(root / "gate.json", report)
    verify(root, capture, report)
    report["complete"] = True
    shutil.copy2(__file__, root / "freeze-gate-assets.py")
    shutil.copy2(Path(__file__).with_name("freeze_baselines.py"), root / "freeze_baselines.py")
    save(root / "gate.json", report)
    print("Complete:", root / "gate.json", flush=True)


if __name__ == "__main__":
    main()
