#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_VTUNE = Path(r"C:\Program Files (x86)\Intel\oneAPI\vtune\2025.10\bin64\vtune.exe")
DEFAULT_TARGET_CANDIDATES = [
    ROOT / "build" / "clang-cl" / "windows" / "x64" / "release" / "call_stream.profile.exe",
    ROOT / "build" / "windows" / "x64" / "release" / "call_stream.profile.exe",
    ROOT / "build" / "msvc" / "windows" / "x64" / "release" / "call_stream.profile.exe",
]
RESULTS_DIR = ROOT / "profiling" / "results"

CASE_PRESETS: dict[str, list[dict[str, str]]] = {
    "focused": [
        {
            "name": "call_stream_ctx_stateless_1024",
            "kind": "call_stream",
            "signature": "void(bench_context&)",
            "workload": "stateless_short",
            "profile_case": "call_stream_ctx_stateless",
        },
        {
            "name": "vector_ctx_stateless_1024",
            "kind": "vector",
            "signature": "void(bench_context&)",
            "workload": "stateless_short",
            "profile_case": "vector_ctx_stateless",
        },
        {
            "name": "call_stream_ctx_payload_1024",
            "kind": "call_stream",
            "signature": "void(bench_context&)",
            "workload": "payload_short",
            "profile_case": "call_stream_ctx_payload",
        },
        {
            "name": "vector_ctx_payload_1024",
            "kind": "vector",
            "signature": "void(bench_context&)",
            "workload": "payload_short",
            "profile_case": "vector_ctx_payload",
        },
        {
            "name": "call_stream_result_stateless_1024",
            "kind": "call_stream",
            "signature": "uint64_t(uint64_t)",
            "workload": "stateless_short",
            "profile_case": "call_stream_result_stateless",
        },
        {
            "name": "vector_result_stateless_1024",
            "kind": "vector",
            "signature": "uint64_t(uint64_t)",
            "workload": "stateless_short",
            "profile_case": "vector_result_stateless",
        },
        {
            "name": "call_stream_result_payload_1024",
            "kind": "call_stream",
            "signature": "uint64_t(uint64_t)",
            "workload": "payload_short",
            "profile_case": "call_stream_result_payload",
        },
        {
            "name": "vector_result_payload_1024",
            "kind": "vector",
            "signature": "uint64_t(uint64_t)",
            "workload": "payload_short",
            "profile_case": "vector_result_payload",
        },
    ],
    "construction": [
        {
            "name": "construct_call_stream_from_zero_trivial_1024",
            "kind": "call_stream",
            "signature": "void()",
            "workload": "construction_from_zero_trivial",
            "profile_case": "construct_call_stream_from_zero_trivial",
        },
        {
            "name": "construct_vector_from_zero_trivial_1024",
            "kind": "vector",
            "signature": "void()",
            "workload": "construction_from_zero_trivial",
            "profile_case": "construct_vector_from_zero_trivial",
        },
        {
            "name": "construct_call_stream_reserved_trivial_1024",
            "kind": "call_stream",
            "signature": "void()",
            "workload": "construction_reserved_trivial",
            "profile_case": "construct_call_stream_reserved_trivial",
        },
        {
            "name": "construct_vector_reserved_trivial_1024",
            "kind": "vector",
            "signature": "void()",
            "workload": "construction_reserved_trivial",
            "profile_case": "construct_vector_reserved_trivial",
        },
    ],
}


def default_profile_target() -> Path:
    for candidate in DEFAULT_TARGET_CANDIDATES:
        if candidate.exists():
            return candidate
    return DEFAULT_TARGET_CANDIDATES[0]


def run_command(command: list[str], cwd: Path, log_path: Path | None = None) -> subprocess.CompletedProcess[str]:
    print("+ " + subprocess.list2cmdline(command), flush=True)
    completed = subprocess.run(
        command,
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if log_path is not None:
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text(completed.stdout, encoding="utf-8", errors="replace")
    if completed.stdout:
        print(completed.stdout, flush=True)
    return completed


def clean_name(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", value).strip("_")


def parse_profile_metrics(stdout_text: str) -> dict[str, Any]:
    allowed_keys = {"case", "calls", "executions", "elapsed_seconds", "calls_per_second", "sink"}
    metrics: dict[str, Any] = {}
    for line in stdout_text.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        if key not in allowed_keys:
            continue
        value = value.strip()
        if key in {"calls", "executions"}:
            try:
                metrics[key] = int(value)
            except ValueError:
                metrics[key] = value
        elif key in {"elapsed_seconds", "calls_per_second"}:
            try:
                metrics[key] = float(value)
            except ValueError:
                metrics[key] = value
        else:
            metrics[key] = value
    return metrics


def calls_per_second(case: dict[str, Any]) -> float | None:
    value = case.get("profile_metrics", {}).get("calls_per_second")
    return value if isinstance(value, float) else None


def collect_case(
    vtune: Path,
    target: Path,
    case: dict[str, str],
    calls: int,
    seconds: float,
    iterations: int,
    batch: int,
    sampling_mode: str,
    sampling_interval: str,
    force: bool,
) -> dict[str, Any]:
    case_dir = RESULTS_DIR / case["name"]
    result_dir = case_dir / "vtune_result"
    case_dir.mkdir(parents=True, exist_ok=True)
    if force and result_dir.exists():
        shutil.rmtree(result_dir)

    target_args = [
        str(target),
        "--case",
        case["profile_case"],
        "--calls",
        str(calls),
        "--batch",
        str(batch),
    ]
    if iterations > 0:
        target_args.extend(["--iterations", str(iterations)])
    else:
        target_args.extend(["--seconds", str(seconds)])

    def build_collect(mode: str) -> list[str]:
        command = [
            str(vtune),
            "-collect",
            "hotspots",
            "-knob",
            f"sampling-mode={mode}",
            "-knob",
            "enable-stack-collection=true",
            "-knob",
            "enable-characterization-insights=false",
        ]
        if mode == "hw":
            command.extend(["-knob", f"sampling-interval={sampling_interval}"])
        command.extend(["-result-dir", str(result_dir), "--"])
        command.extend(target_args)
        return command

    log_path = case_dir / "collect.log"
    completed = run_command(build_collect(sampling_mode), ROOT, log_path)
    used_sampling = sampling_mode
    if completed.returncode != 0 and sampling_mode == "hw":
        if result_dir.exists():
            shutil.rmtree(result_dir, ignore_errors=True)
        fallback_log = case_dir / "collect_sw_fallback.log"
        completed = run_command(build_collect("sw"), ROOT, fallback_log)
        used_sampling = "sw"

    if completed.returncode != 0:
        raise RuntimeError(f"VTune collection failed for {case['name']} with exit code {completed.returncode}")

    stdout_text = completed.stdout
    return {
        **case,
        "result_dir": str(result_dir.relative_to(ROOT)),
        "case_dir": str(case_dir.relative_to(ROOT)),
        "collect_log": str(log_path.relative_to(ROOT)),
        "sampling_mode": used_sampling,
        "profile_metrics": parse_profile_metrics(stdout_text),
        "target_command": target_args,
    }


def export_report(vtune: Path, result_dir: Path, report: str, output: Path, fmt: str, extra: list[str] | None = None) -> bool:
    command = [
        str(vtune),
        "-report",
        report,
        "-result-dir",
        str(result_dir),
        "-format",
        fmt,
        "-report-output",
        str(output),
    ]
    if fmt == "csv":
        command.extend(["-csv-delimiter", "comma"])
    if extra:
        command.extend(extra)
    completed = run_command(command, ROOT, output.with_suffix(output.suffix + ".log"))
    return completed.returncode == 0 and output.exists()


def export_reports(vtune: Path, manifest: dict[str, Any]) -> None:
    for case in manifest["cases"]:
        case_dir = ROOT / case["case_dir"]
        result_dir = ROOT / case["result_dir"]
        report_dir = case_dir / "reports"
        report_dir.mkdir(parents=True, exist_ok=True)
        exports = {
            "summary_txt": report_dir / "summary.txt",
            "hotspots_csv": report_dir / "hotspots.csv",
            "hotspots_txt": report_dir / "hotspots.txt",
            "callstacks_csv": report_dir / "callstacks.csv",
            "top_down_txt": report_dir / "top_down.txt",
        }
        export_report(vtune, result_dir, "summary", exports["summary_txt"], "text", ["-report-width", "220"])
        export_report(vtune, result_dir, "hotspots", exports["hotspots_csv"], "csv", ["-limit", "80"])
        export_report(vtune, result_dir, "hotspots", exports["hotspots_txt"], "text", ["-limit", "80", "-report-width", "260"])
        export_report(vtune, result_dir, "callstacks", exports["callstacks_csv"], "csv", ["-limit", "120"])
        export_report(vtune, result_dir, "top-down", exports["top_down_txt"], "text", ["-limit", "80", "-report-width", "260"])
        case["reports"] = {name: str(path.relative_to(ROOT)) for name, path in exports.items() if path.exists()}


def read_vtune_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    header_index = None
    for index, line in enumerate(lines):
        lowered = line.lower()
        if "function" in lowered and ("cpu time" in lowered or "module" in lowered):
            header_index = index
            break
    if header_index is None:
        return []
    reader = csv.DictReader(lines[header_index:])
    return [{key.strip(): (value or "").strip() for key, value in row.items() if key is not None} for row in reader]


def parse_seconds(value: str) -> float | None:
    match = re.search(r"([0-9]+(?:\.[0-9]+)?)", value.replace(",", ""))
    if not match:
        return None
    return float(match.group(1))


def first_column(row: dict[str, str], candidates: list[str]) -> str:
    lowered = {key.lower(): key for key in row}
    for candidate in candidates:
        for key_lower, original_key in lowered.items():
            if candidate in key_lower:
                return row.get(original_key, "")
    return ""


def summarize_case(case: dict[str, Any]) -> dict[str, Any]:
    hotspots_path = ROOT / case.get("reports", {}).get("hotspots_csv", "")
    rows = read_vtune_csv(hotspots_path)
    summarized_rows = []
    total_cpu = 0.0
    for row in rows:
        function = first_column(row, ["function"])
        module = first_column(row, ["module"])
        source = first_column(row, ["source file", "source"])
        cpu_text = first_column(row, ["cpu time"])
        cpu_seconds = parse_seconds(cpu_text)
        if cpu_seconds is not None:
            total_cpu += cpu_seconds
        if function:
            summarized_rows.append(
                {
                    "function": function,
                    "module": module,
                    "source": source,
                    "cpu_time": cpu_text,
                    "cpu_seconds": cpu_seconds,
                }
            )
    return {
        "name": case["name"],
        "kind": case["kind"],
        "signature": case["signature"],
        "workload": case["workload"],
        "profile_metrics": case.get("profile_metrics", {}),
        "calls_per_second": calls_per_second(case),
        "sampling_mode": case.get("sampling_mode"),
        "top_hotspots": summarized_rows[:20],
        "reported_cpu_seconds_sum": total_cpu,
    }


def write_analysis(manifest: dict[str, Any]) -> None:
    summaries = [summarize_case(case) for case in manifest["cases"]]
    analysis = {
        "generated_at": dt.datetime.now(dt.UTC).isoformat(),
        "manifest": {
            "created_at": manifest.get("created_at"),
            "target": manifest.get("target"),
            "vtune": manifest.get("vtune"),
        },
        "cases": summaries,
    }
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    (RESULTS_DIR / "analysis.json").write_text(json.dumps(analysis, indent=2), encoding="utf-8")

    lines: list[str] = []
    lines.append("# VTune Hotspot Analysis")
    lines.append("")
    lines.append(f"- Generated: {analysis['generated_at']}")
    lines.append(f"- Target: `{manifest.get('target')}`")
    lines.append(f"- VTune: `{manifest.get('vtune')}`")
    lines.append("")
    lines.append("## Profile Throughput")
    lines.append("")
    lines.append("| Case | Calls/s | Executions | Elapsed s | Sampling |")
    lines.append("| --- | ---: | ---: | ---: | --- |")
    for case in summaries:
        metrics = case.get("profile_metrics", {})
        cps = case.get("calls_per_second")
        cps_text = f"{cps:.3e}" if isinstance(cps, float) else ""
        executions = metrics.get("executions", "")
        elapsed = metrics.get("elapsed_seconds", "")
        elapsed_text = f"{elapsed:.3f}" if isinstance(elapsed, float) else str(elapsed)
        lines.append(
            f"| `{case['name']}` | {cps_text} | {executions} | {elapsed_text} | {case.get('sampling_mode', '')} |"
        )
    lines.append("")
    lines.append("## Top Hotspots")
    lines.append("")
    for case in summaries:
        lines.append(f"### {case['name']}")
        if not case["top_hotspots"]:
            lines.append("")
            lines.append("No hotspot CSV rows were parsed.")
            lines.append("")
            continue
        lines.append("")
        lines.append("| CPU Time | Function | Module | Source |")
        lines.append("| ---: | --- | --- | --- |")
        for row in case["top_hotspots"][:10]:
            function = row["function"].replace("|", "\\|")
            module = row["module"].replace("|", "\\|")
            source = row["source"].replace("|", "\\|")
            lines.append(f"| {row['cpu_time']} | `{function}` | `{module}` | `{source}` |")
        lines.append("")

    lines.append("## Initial Interpretation")
    lines.append("")
    lines.append("- The `void(bench_context&)` cases are the baseline for tail-call dispatch cost.")
    lines.append("- The `uint64_t(uint64_t)` cases isolate the return-value callback/context path.")
    lines.append("- Compare `call_stream_*` rows against the matching `vector_*` rows before changing dispatch.")
    if any(case["name"].startswith("construct_") for case in summaries):
        lines.append("- For construction cases, `std::vector<std::move_only_function<...>>::emplace_back` is the path to inspect first when clang-cl is slower; `call_stream` should then be checked against `emit_instruction` and buffer allocation.")
    lines.append("- Use the generated per-case `hotspots.txt` and `callstacks.csv` files for source-level drill-down.")
    lines.append("")
    (RESULTS_DIR / "analysis.md").write_text("\n".join(lines), encoding="utf-8")


def load_manifest() -> dict[str, Any]:
    path = RESULTS_DIR / "manifest.json"
    if not path.exists():
        raise FileNotFoundError(f"missing manifest: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def save_manifest(manifest: dict[str, Any]) -> None:
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    (RESULTS_DIR / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")


def refresh_profile_metrics(manifest: dict[str, Any]) -> None:
    for case in manifest.get("cases", []):
        collect_log = ROOT / case.get("collect_log", "")
        if collect_log.exists():
            case["profile_metrics"] = parse_profile_metrics(collect_log.read_text(encoding="utf-8", errors="replace"))


def command_run(args: argparse.Namespace) -> None:
    vtune = Path(args.vtune)
    target = Path(args.target)
    if not vtune.exists():
        raise FileNotFoundError(vtune)
    if not target.exists():
        raise FileNotFoundError(target)

    cases = CASE_PRESETS[args.preset]
    if args.case:
        wanted = set(args.case)
        cases = [case for case in cases if case["name"] in wanted]
    if not cases:
        raise ValueError("no cases selected")

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    manifest: dict[str, Any] = {
        "created_at": dt.datetime.now(dt.UTC).isoformat(),
        "root": str(ROOT),
        "vtune": str(vtune),
        "target": str(target),
        "preset": args.preset,
        "cases": [],
    }
    for case in cases:
        manifest["cases"].append(
            collect_case(
                vtune=vtune,
                target=target,
                case=case,
                calls=args.calls,
                seconds=args.seconds,
                iterations=args.iterations,
                batch=args.batch,
                sampling_mode=args.sampling_mode,
                sampling_interval=args.sampling_interval,
                force=args.force,
            )
        )
        save_manifest(manifest)

    export_reports(vtune, manifest)
    save_manifest(manifest)
    write_analysis(manifest)


def command_analyze(args: argparse.Namespace) -> None:
    vtune = Path(args.vtune)
    manifest = load_manifest()
    refresh_profile_metrics(manifest)
    export_reports(vtune, manifest)
    save_manifest(manifest)
    write_analysis(manifest)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run and analyze VTune profiles for call_stream profile target.")
    parser.add_argument("--vtune", default=str(DEFAULT_VTUNE), help="Path to vtune.exe")
    subparsers = parser.add_subparsers(dest="command", required=True)

    run_parser = subparsers.add_parser("run", help="Collect VTune profiles and analyze them")
    run_parser.add_argument("--target", default=str(default_profile_target()), help="Path to call_stream.profile.exe")
    run_parser.add_argument("--preset", default="focused", choices=sorted(CASE_PRESETS), help="Case preset")
    run_parser.add_argument("--case", action="append", help="Run only a named case from the preset")
    run_parser.add_argument("--calls", type=int, default=1024, help="Calls encoded in each profile workload")
    run_parser.add_argument("--seconds", type=float, default=8.0, help="Seconds to run each selected case")
    run_parser.add_argument("--iterations", type=int, default=0, help="Fixed outer executions; overrides seconds when positive")
    run_parser.add_argument("--batch", type=int, default=8192, help="Outer executions between clock checks")
    run_parser.add_argument("--sampling-mode", default="hw", choices=["hw", "sw"], help="VTune sampling mode")
    run_parser.add_argument("--sampling-interval", default="0.1", help="Hardware sampling interval in milliseconds")
    run_parser.add_argument("--force", action="store_true", help="Delete existing VTune result dirs for selected cases")
    run_parser.set_defaults(func=command_run)

    analyze_parser = subparsers.add_parser("analyze", help="Export reports and parse existing VTune results")
    analyze_parser.set_defaults(func=command_analyze)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        args.func(args)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
