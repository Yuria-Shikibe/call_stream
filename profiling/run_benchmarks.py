#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import json
import math
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
RESULTS_DIR = ROOT / "benchmark_results"
CHART_PATH = RESULTS_DIR / "speedup_by_signature.png"
POLICY_CHART_PATH = RESULTS_DIR / "noexcept_ratio_by_signature.png"

DEFAULT_TOOLCHAINS = ("clang-cl", "clang", "msvc")
TOOLCHAIN_LABELS = {
    "clang-cl": "clang-cl",
    "clang": "clang",
    "msvc": "MSVC",
}

TEXT_BENCHMARK_RE = re.compile(
    r"^(?P<name>\S+)\s+"
    r"(?P<real>[0-9]+(?:\.[0-9]+)?)\s+(?P<real_unit>ns|us|ms|s)\s+"
    r"(?P<cpu>[0-9]+(?:\.[0-9]+)?)\s+(?P<cpu_unit>ns|us|ms|s)\s+"
    r"(?P<iterations>[0-9]+)\b"
)


def relative_path(path: Path) -> str:
    return path.resolve().relative_to(ROOT).as_posix()


def run_command(command: list[str], log_path: Path | None = None) -> subprocess.CompletedProcess[str]:
    command_line = "+ " + subprocess.list2cmdline(command)
    print(command_line, flush=True)
    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if log_path is not None:
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text(command_line + "\n" + completed.stdout, encoding="utf-8", errors="replace")
    if completed.stdout:
        print(completed.stdout, flush=True)
    return completed


def require_ok(completed: subprocess.CompletedProcess[str]) -> None:
    if completed.returncode != 0:
        raise RuntimeError(f"command failed with exit code {completed.returncode}")


def build_dir(toolchain: str) -> Path:
    return ROOT / "build" / toolchain


def exe_path(toolchain: str, target: str) -> Path:
    return build_dir(toolchain) / "windows" / "x64" / "release" / f"{target}.exe"


def clean_build_dir(toolchain: str) -> None:
    path = build_dir(toolchain).resolve()
    allowed_parent = (ROOT / "build").resolve()
    if path.parent != allowed_parent:
        raise RuntimeError(f"refusing to clean unexpected build path: {path}")
    if path.exists():
        shutil.rmtree(path)


def configure(toolchain: str, clean: bool, force_dispatch_macros: bool) -> None:
    if clean:
        clean_build_dir(toolchain)
    command = [
        "xmake",
        "f",
        "-c",
        "-p",
        "windows",
        "-a",
        "x64",
        "-m",
        "release",
        "--host_project=y",
        f"--toolchain={toolchain}",
        "-o",
        str(build_dir(toolchain)),
        "-y",
    ]
    if force_dispatch_macros:
        command.append("--force_dispatch_macros=y")
    require_ok(
        run_command(
            command,
            RESULTS_DIR / f"{toolchain}_configure.log",
        )
    )


def build(target: str, toolchain: str) -> None:
    require_ok(run_command(["xmake", "-r", "-y", target], RESULTS_DIR / f"{toolchain}_{target}_build.log"))


def run_test(toolchain: str) -> None:
    target = exe_path(toolchain, "call_stream.test")
    require_ok(run_command([str(target)], RESULTS_DIR / f"{toolchain}_test.log"))


def run_benchmark(toolchain: str, min_time: float, repetitions: int, filter_expr: str | None) -> Path:
    output_json = RESULTS_DIR / f"{toolchain}_release_fastest.json"
    output_txt = RESULTS_DIR / f"{toolchain}_release_fastest.txt"
    target = exe_path(toolchain, "call_stream.benchmark")
    command = [
        str(target),
        f"--benchmark_out={output_json}",
        "--benchmark_out_format=json",
        "--benchmark_counters_tabular=true",
        f"--benchmark_min_time={min_time}s",
    ]
    if repetitions > 1:
        command.append(f"--benchmark_repetitions={repetitions}")
        command.append("--benchmark_report_aggregates_only=true")
    if filter_expr:
        command.append(f"--benchmark_filter={filter_expr}")
    require_ok(run_command(command, output_txt))
    return output_json


def split_benchmark_name(name: str) -> tuple[str, str, str, int] | None:
    match = re.fullmatch(r"([^/]+)/([^/]+)/([^/]+)/([0-9]+)(?:_(.+))?", name)
    if not match:
        return None
    kind, signature, workload, calls, aggregate = match.groups()
    if aggregate not in {None, "mean"}:
        return None
    return kind, signature, workload, int(calls)


def cpu_time_ns(benchmark: dict[str, Any]) -> float | None:
    value = benchmark.get("cpu_time")
    unit = benchmark.get("time_unit", "ns")
    if not isinstance(value, (int, float)):
        return None
    multiplier = {
        "ns": 1.0,
        "us": 1_000.0,
        "ms": 1_000_000.0,
        "s": 1_000_000_000.0,
    }.get(str(unit), 1.0)
    return float(value) * multiplier


def load_benchmarks(path: Path) -> list[dict[str, Any]]:
    if path.suffix.lower() == ".json":
        data = json.loads(path.read_text(encoding="utf-8"))
        benchmarks = data.get("benchmarks", [])
        return benchmarks if isinstance(benchmarks, list) else []

    benchmarks: list[dict[str, Any]] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = TEXT_BENCHMARK_RE.match(line.strip())
        if match is None:
            continue
        benchmarks.append(
            {
                "name": match.group("name"),
                "real_time": float(match.group("real")),
                "cpu_time": float(match.group("cpu")),
                "time_unit": match.group("cpu_unit"),
                "iterations": int(match.group("iterations")),
            }
        )
    return benchmarks


def load_pairs(path: Path) -> dict[tuple[str, str, int], dict[str, float]]:
    pairs: dict[tuple[str, str, int], dict[str, float]] = {}
    for bench in load_benchmarks(path):
        name = bench.get("name")
        if not isinstance(name, str):
            continue
        parsed = split_benchmark_name(name)
        if parsed is None:
            continue
        kind, signature, workload, calls = parsed
        if kind not in {"call_stream", "std_vector_move_only_function"}:
            continue
        ns = cpu_time_ns(bench)
        if ns is None:
            continue
        key = (signature, workload, calls)
        slot = "call_stream" if kind == "call_stream" else "vector"
        pairs.setdefault(key, {})[slot] = ns
    return pairs


def load_policy_pairs(path: Path) -> dict[tuple[str, str, int], dict[str, float]]:
    pairs: dict[tuple[str, str, int], dict[str, float]] = {}
    for bench in load_benchmarks(path):
        name = bench.get("name")
        if not isinstance(name, str):
            continue
        parsed = split_benchmark_name(name)
        if parsed is None:
            continue
        kind, signature, workload, calls = parsed
        if kind not in {"call_stream", "call_stream_allow_exception"}:
            continue
        ns = cpu_time_ns(bench)
        if ns is None:
            continue
        key = (signature, workload, calls)
        slot = "noexcept" if kind == "call_stream" else "allow_exception"
        pairs.setdefault(key, {})[slot] = ns
    return pairs


def geomean(values: list[float]) -> float:
    return math.exp(sum(math.log(value) for value in values) / len(values)) if values else float("nan")


def summarize_policy(path: Path) -> dict[str, Any]:
    pairs = load_policy_pairs(path)
    rows: list[dict[str, Any]] = []
    ratios: list[float] = []
    by_signature: dict[str, list[float]] = {}
    faster = 0
    for key in sorted(pairs):
        values = pairs[key]
        if "noexcept" not in values or "allow_exception" not in values:
            continue
        signature, workload, calls = key
        noexcept_ns = values["noexcept"]
        allow_exception_ns = values["allow_exception"]
        ratio = allow_exception_ns / noexcept_ns
        ratios.append(ratio)
        by_signature.setdefault(signature, []).append(ratio)
        if ratio > 1.0:
            faster += 1
        rows.append(
            {
                "signature": signature,
                "workload": workload,
                "calls": calls,
                "noexcept_ns": noexcept_ns,
                "allow_exception_ns": allow_exception_ns,
                "noexcept_ns_per_call": noexcept_ns / calls,
                "allow_exception_ns_per_call": allow_exception_ns / calls,
                "ratio": ratio,
            }
        )
    return {
        "rows": rows,
        "faster": faster,
        "total": len(rows),
        "geomean": geomean(ratios),
        "by_signature": {signature: geomean(values) for signature, values in sorted(by_signature.items())},
    }


def summarize(path: Path, toolchain: str, label: str | None = None) -> dict[str, Any]:
    pairs = load_pairs(path)
    rows: list[dict[str, Any]] = []
    speedups: list[float] = []
    by_signature: dict[str, list[float]] = {}
    faster = 0
    for key in sorted(pairs):
        values = pairs[key]
        if "call_stream" not in values or "vector" not in values:
            continue
        signature, workload, calls = key
        call_ns = values["call_stream"]
        vector_ns = values["vector"]
        speedup = vector_ns / call_ns
        speedups.append(speedup)
        by_signature.setdefault(signature, []).append(speedup)
        if speedup > 1.0:
            faster += 1
        rows.append(
            {
                "signature": signature,
                "workload": workload,
                "calls": calls,
                "call_stream_ns": call_ns,
                "vector_ns": vector_ns,
                "call_stream_ns_per_call": call_ns / calls,
                "vector_ns_per_call": vector_ns / calls,
                "speedup": speedup,
            }
        )
    return {
        "toolchain": toolchain,
        "label": label or TOOLCHAIN_LABELS.get(toolchain, toolchain),
        "json": relative_path(path) if path.suffix.lower() == ".json" else None,
        "txt": relative_path(path.with_suffix(".txt")) if path.suffix.lower() == ".json" else relative_path(path),
        "rows": rows,
        "faster": faster,
        "total": len(rows),
        "geomean": geomean(speedups),
        "by_signature": {signature: geomean(values) for signature, values in sorted(by_signature.items())},
        "policy": summarize_policy(path),
    }


def plot_summaries(summaries: list[dict[str, Any]], output_path: Path = CHART_PATH) -> None:
    if not summaries:
        return
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise RuntimeError("matplotlib is required to generate benchmark charts; run `python -m pip install matplotlib`") from exc

    signatures = sorted({signature for summary in summaries for signature in summary["by_signature"]})
    categories = ["Overall", *signatures]
    x_positions = list(range(len(categories)))
    width = min(0.32, 0.8 / max(1, len(summaries)))
    colors = ["#2f6f9f", "#c7662e", "#3f8f5f", "#8a5f9e"]

    fig_width = max(9.0, 1.45 * len(categories) + 2.5)
    fig, ax = plt.subplots(figsize=(fig_width, 5.6), dpi=160)

    for index, summary in enumerate(summaries):
        offset = (index - (len(summaries) - 1) / 2) * width
        values = [summary["geomean"], *[summary["by_signature"].get(signature, float("nan")) for signature in signatures]]
        bars = ax.bar(
            [x + offset for x in x_positions],
            values,
            width,
            label=summary["label"],
            color=colors[index % len(colors)],
            edgecolor="#1f2933",
            linewidth=0.5,
        )
        for bar, value in zip(bars, values):
            if not math.isfinite(value):
                continue
            label_y = value + 0.025 if value >= 1.0 else value - 0.045
            va = "bottom" if value >= 1.0 else "top"
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                label_y,
                f"{value:.2f}x",
                ha="center",
                va=va,
                fontsize=8,
                color="#1f2933",
            )

    all_values = [
        value
        for summary in summaries
        for value in [summary["geomean"], *summary["by_signature"].values()]
        if math.isfinite(value)
    ]
    min_value = min(all_values, default=0.8)
    max_value = max(all_values, default=1.2)
    lower = max(0.0, min(0.75, min_value - 0.15))
    upper = max(1.25, max_value + 0.2)

    ax.axhline(1.0, color="#3f3f46", linewidth=1.0, linestyle="--", alpha=0.75)
    ax.text(len(categories) - 0.55, 1.015, "1.00x parity", fontsize=8, color="#3f3f46")
    ax.set_ylim(lower, upper)
    ax.set_ylabel("Speedup vs std::vector<std::move_only_function> (x)")
    ax.set_title("call_stream benchmark speedup by toolchain and signature")
    ax.set_xticks(x_positions)
    ax.set_xticklabels(categories, rotation=18, ha="right")
    ax.grid(axis="y", alpha=0.25, linewidth=0.8)
    ax.legend(frameon=False, ncol=min(len(summaries), 3), loc="upper left")
    ax.margins(x=0.03)

    fig.tight_layout()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path)
    plt.close(fig)
    print(f"wrote {relative_path(output_path)}", flush=True)


def plot_policy_summaries(summaries: list[dict[str, Any]], output_path: Path = POLICY_CHART_PATH) -> None:
    summaries = [summary for summary in summaries if summary["policy"]["total"]]
    if not summaries:
        return
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise RuntimeError("matplotlib is required to generate benchmark charts; run `python -m pip install matplotlib`") from exc

    signatures = sorted({signature for summary in summaries for signature in summary["policy"]["by_signature"]})
    categories = ["Overall", *signatures]
    x_positions = list(range(len(categories)))
    width = min(0.24, 0.82 / max(1, len(summaries)))
    colors = ["#2f6f9f", "#c7662e", "#3f8f5f", "#8a5f9e", "#5f6f8f"]

    fig_width = max(10.0, 1.55 * len(categories) + 3.2)
    fig, ax = plt.subplots(figsize=(fig_width, 5.8), dpi=160)

    for index, summary in enumerate(summaries):
        offset = (index - (len(summaries) - 1) / 2) * width
        policy = summary["policy"]
        values = [policy["geomean"], *[policy["by_signature"].get(signature, float("nan")) for signature in signatures]]
        bars = ax.bar(
            [x + offset for x in x_positions],
            values,
            width,
            label=summary["label"],
            color=colors[index % len(colors)],
            edgecolor="#1f2933",
            linewidth=0.5,
        )
        for bar, value in zip(bars, values):
            if not math.isfinite(value):
                continue
            label_y = value + 0.025 if value >= 1.0 else value - 0.045
            va = "bottom" if value >= 1.0 else "top"
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                label_y,
                f"{value:.2f}x",
                ha="center",
                va=va,
                fontsize=8,
                color="#1f2933",
            )

    all_values = [
        value
        for summary in summaries
        for value in [summary["policy"]["geomean"], *summary["policy"]["by_signature"].values()]
        if math.isfinite(value)
    ]
    min_value = min(all_values, default=0.8)
    max_value = max(all_values, default=1.2)
    lower = max(0.0, min(0.75, min_value - 0.15))
    upper = max(1.25, max_value + 0.2)

    ax.axhline(1.0, color="#3f3f46", linewidth=1.0, linestyle="--", alpha=0.75)
    ax.text(len(categories) - 0.55, 1.015, "1.00x parity", fontsize=8, color="#3f3f46")
    ax.set_ylim(lower, upper)
    ax.set_ylabel("allow-exception / noexcept CPU time (x)")
    ax.set_title("call_stream noexcept policy ratio by toolchain and signature")
    ax.set_xticks(x_positions)
    ax.set_xticklabels(categories, rotation=18, ha="right")
    ax.grid(axis="y", alpha=0.25, linewidth=0.8)
    ax.legend(frameon=False, ncol=min(len(summaries), 4), loc="upper left")
    ax.margins(x=0.03)

    fig.tight_layout()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path)
    plt.close(fig)
    print(f"wrote {relative_path(output_path)}", flush=True)


def format_ns(value: float) -> str:
    if value >= 10_000:
        return f"{value:,.0f}"
    if value >= 100:
        return f"{value:,.1f}"
    return f"{value:,.3f}"


def parse_source_arg(source: str) -> tuple[str, Path]:
    if "=" not in source:
        raise ValueError(f"source must use LABEL=PATH format: {source}")
    label, raw_path = source.split("=", 1)
    label = label.strip()
    if not label:
        raise ValueError(f"source label is empty: {source}")
    path = Path(raw_path.strip())
    if not path.is_absolute():
        path = ROOT / path
    if not path.exists():
        raise FileNotFoundError(path)
    return label, path


def summarize_sources(args: argparse.Namespace) -> list[dict[str, Any]]:
    summaries: list[dict[str, Any]] = []
    for source in args.source:
        label, path = parse_source_arg(source)
        summaries.append(summarize(path, label, label))
    return summaries


def format_markdown(summaries: list[dict[str, Any]], args: argparse.Namespace) -> str:
    lines: list[str] = []
    lines.append("# call_stream benchmark summary")
    lines.append("")
    lines.append(f"Generated: {dt.datetime.now().isoformat(timespec='seconds')}")
    if args.source:
        lines.append("")
        lines.append(
            "Imported `--source` benchmark files are summarized alongside local runs; "
            "their host environment is the one preserved in each raw source file."
        )
    lines.append("")
    lines.append("## Environment")
    lines.append("")
    lines.append(f"- OS: {platform.platform()}")
    lines.append(f"- CPU: {platform.processor() or 'unknown'}")
    lines.append(f"- xmake mode: release")
    lines.append(f"- xmake optimize: fastest")
    lines.append(f"- benchmark_min_time: {args.min_time}")
    lines.append(f"- benchmark_repetitions: {args.repetitions}")
    if args.force_dispatch_macros:
        lines.append(
            "- force_dispatch_macros: yes "
            "(`MO_YANXI_CALL_STREAM_USE_TAIL_DISPATCH=1`, "
            "`MO_YANXI_CALL_STREAM_USE_SCALAR_RESULT_DISPATCH=1`)"
        )
    if args.filter:
        lines.append(f"- benchmark_filter: `{args.filter}`")
    lines.append("")
    lines.append("Speedup is `std::vector<std::move_only_function>` CPU time divided by `call_stream` CPU time.")
    lines.append(
        "Exception-policy ratio is `call_stream_allow_exception` CPU time divided by `call_stream` CPU time; "
        "values above 1.00x mean the `noexcept` stream is faster."
    )
    lines.append("")
    for summary in summaries:
        lines.append(f"## {summary['label']}")
        lines.append("")
        if summary["json"]:
            lines.append(f"- JSON: [{summary['json']}](../{summary['json']})")
        if summary["txt"]:
            lines.append(f"- Text: [{summary['txt']}](../{summary['txt']})")
        lines.append("")
        if summary["total"]:
            by_sig = ", ".join(f"`{sig}` {value:.2f}x" for sig, value in summary["by_signature"].items())
            lines.append(
                f"`call_stream` is faster in {summary['faster']}/{summary['total']} vector cases; "
                f"geomean speedup is {summary['geomean']:.2f}x. By signature: {by_sig}."
            )
            lines.append("")
            lines.append(
                "| Signature | Workload | Calls | call_stream CPU ns | vector CPU ns | "
                "call_stream ns/call | vector ns/call | Speedup |"
            )
            lines.append("|---|---|---:|---:|---:|---:|---:|---:|")
            for row in summary["rows"]:
                lines.append(
                    f"| `{row['signature']}` | `{row['workload']}` | {row['calls']} | "
                    f"{format_ns(row['call_stream_ns'])} | {format_ns(row['vector_ns'])} | "
                    f"{format_ns(row['call_stream_ns_per_call'])} | {format_ns(row['vector_ns_per_call'])} | "
                    f"{row['speedup']:.2f}x |"
                )
            lines.append("")

        policy = summary["policy"]
        if policy["total"]:
            by_sig = ", ".join(f"`{sig}` {value:.2f}x" for sig, value in policy["by_signature"].items())
            lines.append(
                f"`noexcept` is faster than allow-exception in {policy['faster']}/{policy['total']} cases; "
                f"geomean ratio is {policy['geomean']:.2f}x. By signature: {by_sig}."
            )
            lines.append("")
            lines.append(
                "| Signature | Workload | Calls | noexcept CPU ns | allow-exception CPU ns | "
                "noexcept ns/call | allow-exception ns/call | Ratio |"
            )
            lines.append("|---|---|---:|---:|---:|---:|---:|---:|")
            for row in policy["rows"]:
                lines.append(
                    f"| `{row['signature']}` | `{row['workload']}` | {row['calls']} | "
                    f"{format_ns(row['noexcept_ns'])} | {format_ns(row['allow_exception_ns'])} | "
                    f"{format_ns(row['noexcept_ns_per_call'])} | {format_ns(row['allow_exception_ns_per_call'])} | "
                    f"{row['ratio']:.2f}x |"
                )
            lines.append("")
    return "\n".join(lines)


def command_run(args: argparse.Namespace) -> None:
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    summaries = summarize_sources(args)
    for toolchain in args.toolchain:
        configure(toolchain, args.clean, args.force_dispatch_macros)
        build("call_stream.test", toolchain)
        run_test(toolchain)
        configure(toolchain, args.clean, args.force_dispatch_macros)
        build("call_stream.benchmark", toolchain)
        json_path = run_benchmark(toolchain, args.min_time, args.repetitions, args.filter)
        summaries.append(summarize(json_path, toolchain))
    summary_path = RESULTS_DIR / "summary.md"
    summary_path.write_text(format_markdown(summaries, args), encoding="utf-8")
    print(f"wrote {relative_path(summary_path)}", flush=True)
    if args.plot:
        plot_summaries(summaries)
        plot_policy_summaries(summaries)


def command_summarize(args: argparse.Namespace) -> None:
    summaries = summarize_sources(args)
    for toolchain in args.toolchain:
        path = RESULTS_DIR / f"{toolchain}_release_fastest.json"
        if not path.exists():
            raise FileNotFoundError(path)
        summaries.append(summarize(path, toolchain))
    summary_path = RESULTS_DIR / "summary.md"
    summary_path.write_text(format_markdown(summaries, args), encoding="utf-8")
    print(f"wrote {relative_path(summary_path)}", flush=True)
    if args.plot:
        plot_summaries(summaries)
        plot_policy_summaries(summaries)


def command_plot(args: argparse.Namespace) -> None:
    summaries = summarize_sources(args)
    for toolchain in args.toolchain:
        path = RESULTS_DIR / f"{toolchain}_release_fastest.json"
        if not path.exists():
            raise FileNotFoundError(path)
        summaries.append(summarize(path, toolchain))
    plot_summaries(summaries)
    plot_policy_summaries(summaries)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Build, test, benchmark, and summarize call_stream.")
    parser.add_argument(
        "--toolchain",
        action="append",
        choices=DEFAULT_TOOLCHAINS,
        default=[],
        help="Toolchain to run. Defaults to clang-cl, clang, and msvc.",
    )
    parser.add_argument("--min-time", type=float, default=0.08, help="Google Benchmark minimum time per benchmark.")
    parser.add_argument("--repetitions", type=int, default=1, help="Google Benchmark repetitions.")
    parser.add_argument("--filter", help="Optional Google Benchmark filter.")
    parser.add_argument(
        "--source",
        action="append",
        default=[],
        metavar="LABEL=PATH",
        help="Additional benchmark JSON or Google Benchmark text output to include in summaries and plots.",
    )
    parser.add_argument(
        "--force-dispatch-macros",
        action="store_true",
        help="Configure xmake with MO_YANXI_CALL_STREAM_USE_TAIL_DISPATCH=1 and "
        "MO_YANXI_CALL_STREAM_USE_SCALAR_RESULT_DISPATCH=1.",
    )
    parser.add_argument("--no-plot", action="store_false", dest="plot", help="Skip PNG chart generation.")
    parser.add_argument(
        "--no-clean",
        action="store_false",
        dest="clean",
        help="Keep the existing build directory instead of cleaning it before each target.",
    )
    parser.set_defaults(clean=True)
    subparsers = parser.add_subparsers(dest="command", required=True)
    run_parser = subparsers.add_parser("run", help="Configure, build, test, run benchmarks, and summarize.")
    run_parser.set_defaults(func=command_run)
    summarize_parser = subparsers.add_parser("summarize", help="Summarize existing benchmark JSON files.")
    summarize_parser.set_defaults(func=command_summarize)
    plot_parser = subparsers.add_parser("plot", help="Generate a PNG chart from existing benchmark JSON files.")
    plot_parser.set_defaults(func=command_plot)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    if not args.toolchain:
        args.toolchain = list(DEFAULT_TOOLCHAINS)
    try:
        args.func(args)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
