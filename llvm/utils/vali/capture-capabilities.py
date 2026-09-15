#!/usr/bin/env python3
"""Record Vali frontend and object-emission coverage for a compiler build.

Failures are recorded, not silently skipped: some targets have only frontend
hooks in the original port. Compare these reports alongside lit tests when
qualifying a forward port. Success here does not establish runtime support.
"""

import argparse
import json
import subprocess
from pathlib import Path


TRIPLES = (
    "i386-uml-vali", "x86_64-uml-vali",
    "armv7-uml-vali", "thumbv7-uml-vali",
    "armebv7-uml-vali", "thumbebv7-uml-vali",
    "aarch64-uml-vali", "aarch64_be-uml-vali",
    "mips-uml-vali", "mipsel-uml-vali",
    "mips64-uml-vali", "mips64el-uml-vali",
)


def run(command, log, timeout):
    try:
        result = subprocess.run(command, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=timeout)
        log.write_bytes(result.stdout)
        return {"exit_code": result.returncode, "log": str(log)}
    except subprocess.TimeoutExpired as error:
        log.write_bytes(error.stdout or b"")
        return {"exit_code": None, "timeout": True, "log": str(log)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=60)
    args = parser.parse_args()
    clang = args.bin_dir.resolve() / "clang"
    if not clang.is_file():
        parser.error(f"compiler not found: {clang}")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = output / "probe.c"
    source.write_text("long vali_probe(long x) { return x + 1; }\n")
    version = subprocess.check_output([str(clang), "--version"], text=True)
    report = {"compiler": str(clang), "version": version, "targets": {}}
    for triple in TRIPLES:
        directory = output / triple
        directory.mkdir(exist_ok=True)
        common = [str(clang), "-cc1", "-triple", triple]
        modes = {
            "macros": ["-E", "-dM"],
            "syntax": ["-fsyntax-only"],
            "ir": ["-emit-llvm", "-o", str(directory / "probe.ll")],
            "assembly": ["-S", "-o", str(directory / "probe.s")],
            "object": ["-emit-obj", "-o", str(directory / "probe.o")],
        }
        results = {}
        for mode, flags in modes.items():
            result = run(common + flags + [str(source)],
                         directory / f"{mode}.log", args.timeout)
            results[mode] = result
            print(f"{triple}: {mode}: {result['exit_code']}", flush=True)
        obj = directory / "probe.o"
        if results["object"]["exit_code"] == 0 and obj.is_file():
            results["object"]["prefix_hex"] = obj.read_bytes()[:20].hex()
        report["targets"][triple] = results
        # Preserve completed target results if a later invocation is interrupted.
        (output / "capabilities.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
