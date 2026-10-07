#!/usr/bin/env python3
"""Record LLVM source coverage of the v1 core. No target percentage imposed."""
import argparse
import datetime
import hashlib
import json
import os
import pathlib
import platform
import shlex
import shutil
import subprocess


def tool(name):
    if platform.system() == "Darwin":
        return subprocess.check_output(["xcrun", "--find", name], text=True).strip()
    found = shutil.which(name)
    if found is None:
        raise SystemExit(f"{name} required: install matching LLVM tools")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build/coverage")
    parser.add_argument("--cc", default="clang")
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parent.parent
    dest = (root / args.build_dir).resolve()
    dest.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    if platform.system() == "Darwin" and not env.get("SDKROOT"):
        env["SDKROOT"] = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
    sources = ["src/ecu_core.c", "src/ecu_protocol.c", "src/ecu_time.c", "src/ecu_policy.c"]
    run_records = []

    def run(command, extra=None):
        actual_env = env.copy()
        actual_env.update(extra or {})
        result = subprocess.run([str(x) for x in command], cwd=root, env=actual_env,
                                text=True, capture_output=True)
        record = {"command": [str(x) for x in command], "exit_code": result.returncode,
                  "stdout": result.stdout, "stderr": result.stderr}
        run_records.append(record)
        (dest / "commands.json").write_text(json.dumps(run_records, indent=2) + "\n")
        if result.returncode:
            print(result.stdout, end="")
            print(result.stderr, end="")
            raise SystemExit(result.returncode)
        return result.stdout

    flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-Iinclude",
             "-O0", "-g", "-fprofile-instr-generate", "-fcoverage-mapping"]
    binary, raw, profile = dest / "test_core", dest / "core.profraw", dest / "core.profdata"
    run(shlex.split(args.cc) + flags + ["tests/test_core.c"] + sources + ["-o", binary])
    run([binary], {"LLVM_PROFILE_FILE": str(raw)})
    profdata, cov = tool("llvm-profdata"), tool("llvm-cov")
    run([profdata, "merge", "-sparse", raw, "-o", profile])
    command = [cov, "export", "--summary-only", binary,
               f"--instr-profile={profile}"] + [root / p for p in sources]
    exported = json.loads(run(command))
    (dest / "llvm-coverage-export.json").write_text(json.dumps(exported, indent=2) + "\n")
    data = exported["data"][0]
    expected_paths = {str((root / p).resolve()) for p in sources}
    actual_paths = {str(pathlib.Path(f["filename"]).resolve()) for f in data["files"]}
    assert actual_paths == expected_paths, "Coverage denominator must be exactly the four documented C modules"
    metrics = {}
    for key in ("lines", "branches", "functions", "regions"):
        total = data["totals"][key]
        assert total["count"] == sum(f["summary"][key]["count"] for f in data["files"])
        assert total["covered"] == sum(f["summary"][key]["covered"] for f in data["files"])
        metrics[key] = {"covered": total["covered"], "total": total["count"],
                        "percent": round(100 * total["covered"] / total["count"], 2)
                        if total["count"] else None}
    compiler = run(shlex.split(args.cc) + ["--version"]).strip()
    llvm = run([cov, "--version"]).strip()
    summary = {
        "measured_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "base_revision": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "working_tree_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=root, text=True).strip()),
        "platform": platform.platform(), "compiler": compiler, "llvm_cov": llvm,
        "instrumentation_flags": flags, "source_scope": sources,
        "source_sha256": {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in sources},
        "test_runner": "tests/test_core.c", "named_test_functions_invoked": 20,
        "metrics": metrics,
        "exclusions": ["legacy src/ecu.c", "demo/scenario runners", "test code",
                       "UART transport parser", "ESP32 adapter"],
        "limits": ["Source execution coverage, not requirements or hardware coverage",
                   "No bug-freedom, safety certification or real-time guarantee",
                   "Counters from existing deterministic core tests; no fuzz campaign"]
    }
    (dest / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    run([cov, "report", binary, f"--instr-profile={profile}"] + [root / p for p in sources])
    run([cov, "show", binary, f"--instr-profile={profile}", "--format=html",
         f"--output-dir={dest / 'html'}"] + [root / p for p in sources])
    print(json.dumps(metrics, indent=2))


if __name__ == "__main__":
    main()
