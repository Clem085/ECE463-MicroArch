#!/usr/bin/env python3
"""
Generate predictor sweep data and plots for Project 2.

This script reproduces the experiments requested in the project report template:
  * Bimodal predictor sweep (m = 7..20) on gcc, jpeg, and perl traces.
  * Gshare predictor sweep (m = 7..20, n = 0..m) on the gcc trace.

Hybrid sweeps can be added by providing a JSON configuration file; see
`load_hybrid_config` for details.
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

ROOT_DIR = Path(__file__).resolve().parent.parent
SIM_BIN = ROOT_DIR / "sim"
GRAPH_ROOT = ROOT_DIR / "graphs"
RESULTS_DIR = GRAPH_ROOT / "results"
FIGS_DIR = GRAPH_ROOT / "figs"

TRACES = {
    "gcc": "traces/gcc_trace.txt",
    "jpeg": "traces/jpeg_trace.txt",
    "perl": "traces/perl_trace.txt",
}

STAT_PATTERNS = {
    "predictions": re.compile(r"number of predictions:\s+(\d+)"),
    "mispredictions": re.compile(r"number of mispredictions:\s+(\d+)"),
    "misprediction_rate": re.compile(r"misprediction rate:\s+([0-9]*\.?[0-9]+)%"),
}


@dataclass
class SimulationResult:
    mode: str
    args: Sequence[str]
    predictions: int
    mispredictions: int
    misprediction_rate: float


def ensure_paths() -> None:
    GRAPH_ROOT.mkdir(parents=True, exist_ok=True)
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    FIGS_DIR.mkdir(parents=True, exist_ok=True)


def run_simulation(arguments: Sequence[str]) -> SimulationResult:
    completed = subprocess.run(
        arguments,
        cwd=ROOT_DIR,
        capture_output=True,
        text=True,
        check=True,
    )

    stats: Dict[str, str] = {}
    for key, pattern in STAT_PATTERNS.items():
        match = pattern.search(completed.stdout)
        if not match:
            raise RuntimeError(f"Failed to parse {key} from simulator output.")
        stats[key] = match.group(1)

    return SimulationResult(
        mode=arguments[1],
        args=arguments[2:],
        predictions=int(stats["predictions"]),
        mispredictions=int(stats["mispredictions"]),
        misprediction_rate=float(stats["misprediction_rate"]),
    )


def bimodal_sweep(m_values: Iterable[int], traces: Dict[str, str]) -> Dict[str, List[Dict[str, float]]]:
    results: Dict[str, List[Dict[str, float]]] = {bench: [] for bench in traces}
    for bench, trace_name in traces.items():
        trace_path = trace_name
        for m in m_values:
            sim_args = ["./sim", "bimodal", str(m), trace_path]
            sim_result = run_simulation(sim_args)
            results[bench].append(
                {
                    "benchmark": bench,
                    "m": m,
                    "predictions": sim_result.predictions,
                    "mispredictions": sim_result.mispredictions,
                    "misprediction_rate": sim_result.misprediction_rate,
                }
            )
    return results


def gshare_sweep(m_values: Iterable[int]) -> List[Dict[str, float]]:
    data: List[Dict[str, float]] = []
    trace_path = TRACES["gcc"]
    for m in m_values:
        for n in range(0, m + 1):
            sim_args = ["./sim", "gshare", str(m), str(n), trace_path]
            sim_result = run_simulation(sim_args)
            data.append(
                {
                    "benchmark": "gcc",
                    "m": m,
                    "n": n,
                    "predictions": sim_result.predictions,
                    "mispredictions": sim_result.mispredictions,
                    "misprediction_rate": sim_result.misprediction_rate,
                }
            )
    return data


def load_hybrid_config(config_path: Optional[Path]) -> List[Tuple[int, int, int, int]]:
    """
    Load hybrid sweep configurations from a JSON file with the format:
    {
        "trace": "gcc_trace.txt",
        "parameters": [
            {"k": 8, "m1": 14, "n": 10, "m2": 5},
            ...
        ]
    }
    If no config is provided, a small default set inspired by the validation
    runs is returned.
    """
    if config_path:
        with config_path.open("r", encoding="utf-8") as f:
            payload = json.load(f)
        combos = []
        for entry in payload.get("parameters", []):
            combos.append((entry["k"], entry["m1"], entry["n"], entry["m2"]))
        return combos

    return [
        (8, 14, 10, 5),
        (8, 12, 8, 9),
        (8, 16, 12, 7),
        (10, 14, 10, 5),
    ]


def hybrid_sweep(configs: Sequence[Tuple[int, int, int, int]], trace_name: str) -> List[Dict[str, float]]:
    data: List[Dict[str, float]] = []
    for k, m1, n, m2 in configs:
        if n > m1:
            raise ValueError(f"Hybrid configuration invalid: n ({n}) cannot exceed m1 ({m1}).")
        sim_args = ["./sim", "hybrid", str(k), str(m1), str(n), str(m2), trace_name]
        sim_result = run_simulation(sim_args)
        data.append(
            {
                "benchmark": Path(trace_name).stem.replace("_trace", ""),
                "k": k,
                "m1": m1,
                "n": n,
                "m2": m2,
                "predictions": sim_result.predictions,
                "mispredictions": sim_result.mispredictions,
                "misprediction_rate": sim_result.misprediction_rate,
            }
        )
    return data


def write_csv(path: Path, fieldnames: Sequence[str], rows: Sequence[Dict[str, float]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def plot_bimodal(results: Dict[str, List[Dict[str, float]]]) -> None:
    plt = try_import_matplotlib()
    if plt is None:
        return

    for bench, rows in results.items():
        xs = [row["m"] for row in rows]
        ys = [row["misprediction_rate"] for row in rows]
        plt.figure()
        plt.plot(xs, ys, marker="o")
        plt.title(f"{bench}, bimodal")
        plt.xlabel("m (table index bits)")
        plt.ylabel("Misprediction rate (%)")
        plt.grid(True, linestyle="--", linewidth=0.5)
        plt.xticks(xs)
        plt.tight_layout()
        plt.savefig(FIGS_DIR / f"{bench}_bimodal.png", dpi=200)
        plt.close()


def plot_gshare(rows: List[Dict[str, float]]) -> None:
    plt = try_import_matplotlib()
    if plt is None:
        return

    by_m: Dict[int, List[Tuple[int, float]]] = {}
    for row in rows:
        by_m.setdefault(row["m"], []).append((row["n"], row["misprediction_rate"]))

    plt.figure()
    for m in sorted(by_m):
        pairs = sorted(by_m[m], key=lambda item: item[0])
        xs = [n for n, _ in pairs]
        ys = [rate for _, rate in pairs]
        plt.plot(xs, ys, marker="o", label=f"m={m}")

    plt.title("gcc, gshare")
    plt.xlabel("n (global history bits)")
    plt.ylabel("Misprediction rate (%)")
    plt.grid(True, linestyle="--", linewidth=0.5)
    plt.legend(ncol=2, fontsize="small")
    plt.tight_layout()
    plt.savefig(FIGS_DIR / "gcc_gshare.png", dpi=200)
    plt.close()


def plot_hybrid(rows: List[Dict[str, float]]) -> None:
    if not rows:
        return
    plt = try_import_matplotlib()
    if plt is None:
        return

    plt.figure()
    for row in rows:
        label = f"k={row['k']}, m1={row['m1']}, m2={row['m2']}"
        plt.scatter(row["n"], row["misprediction_rate"], label=label)
    plt.title("Hybrid predictor sweep")
    plt.xlabel("n (global history bits)")
    plt.ylabel("Misprediction rate (%)")
    plt.grid(True, linestyle="--", linewidth=0.5)
    plt.legend(fontsize="small", bbox_to_anchor=(1.05, 1), loc="upper left")
    plt.tight_layout()
    plt.savefig(FIGS_DIR / "hybrid_overview.png", dpi=200)
    plt.close()


def try_import_matplotlib():
    try:
        import matplotlib.pyplot as plt  # type: ignore
    except ImportError:
        print("matplotlib not available; skipping plot generation.", file=sys.stderr)
        return None
    return plt


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run Project 2 predictor sweeps.")
    parser.add_argument(
        "--bimodal",
        action="store_true",
        help="Run only the bimodal sweeps (default: run all unless filters are provided).",
    )
    parser.add_argument(
        "--gshare",
        action="store_true",
        help="Run only the gshare sweeps (default: run all unless filters are provided).",
    )
    parser.add_argument(
        "--hybrid",
        action="store_true",
        help="Run hybrid sweeps in addition to other requested sweeps.",
    )
    parser.add_argument(
        "--no-plots",
        action="store_true",
        help="Skip plot generation (CSV data will still be produced).",
    )
    parser.add_argument(
        "--hybrid-config",
        type=Path,
        help="Optional JSON file defining hybrid sweep configurations.",
    )
    parser.add_argument(
        "--hybrid-trace",
        default=TRACES["gcc"],
        help="Trace file to use for hybrid sweeps (default: gcc trace).",
    )
    return parser.parse_args()


def main() -> None:
    if not SIM_BIN.exists():
        print("Simulator binary not found. Run 'make' before launching sweeps.", file=sys.stderr)
        sys.exit(1)

    ensure_paths()

    args = parse_args()
    filters = {flag for flag, enabled in (("bimodal", args.bimodal), ("gshare", args.gshare), ("hybrid", args.hybrid)) if enabled}
    run_all = not filters

    if run_all or "bimodal" in filters:
        bimodal_data = bimodal_sweep(range(7, 21), TRACES)
        for bench, rows in bimodal_data.items():
            csv_path = RESULTS_DIR / f"bimodal_{bench}.csv"
            write_csv(csv_path, ["benchmark", "m", "predictions", "mispredictions", "misprediction_rate"], rows)
        if not args.no_plots:
            plot_bimodal(bimodal_data)

    if run_all or "gshare" in filters:
        gshare_data = gshare_sweep(range(7, 21))
        csv_path = RESULTS_DIR / "gshare_gcc.csv"
        write_csv(csv_path, ["benchmark", "m", "n", "predictions", "mispredictions", "misprediction_rate"], gshare_data)
        if not args.no_plots:
            plot_gshare(gshare_data)

    if args.hybrid or (run_all and args.hybrid_config):
        combos = load_hybrid_config(args.hybrid_config)
        hybrid_data = hybrid_sweep(combos, args.hybrid_trace)
        csv_path = RESULTS_DIR / "hybrid_summary.csv"
        write_csv(
            csv_path,
            ["benchmark", "k", "m1", "n", "m2", "predictions", "mispredictions", "misprediction_rate"],
            hybrid_data,
        )
        if not args.no_plots:
            plot_hybrid(hybrid_data)


if __name__ == "__main__":
    main()
