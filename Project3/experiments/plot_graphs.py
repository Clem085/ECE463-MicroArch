#!/usr/bin/env python3
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent
CSV_PATH = ROOT / "results.csv"
GRAPH_DIR = ROOT / "graphs"
GRAPH_DIR.mkdir(exist_ok=True)


def load_results():
    results = []
    with CSV_PATH.open() as f:
        reader = csv.DictReader(f)
        for row in reader:
            row["rob"] = int(row["rob"])
            row["iq"] = int(row["iq"])
            row["width"] = int(row["width"])
            row["cycles"] = int(row["cycles"])
            row["inst_count"] = int(row["inst_count"])
            row["ipc"] = float(row["ipc"])
            results.append(row)
    return results


def compute_optimal_iq(results):
    optimal = defaultdict(dict)  # benchmark -> width -> iq
    for bench in {r["benchmark"] for r in results}:
        for width in {r["width"] for r in results}:
            subset = [r for r in results if r["benchmark"] == bench and r["width"] == width and r["rob"] == 512]
            if not subset:
                continue
            subset.sort(key=lambda r: r["iq"])
            max_ipc = max(r["ipc"] for r in subset if r["iq"] == 256) if any(r["iq"] == 256 for r in subset) else max(r["ipc"] for r in subset)
            threshold = 0.94 * max_ipc
            chosen = min((r for r in subset if r["ipc"] >= threshold), key=lambda r: r["iq"])
            optimal[bench][width] = chosen["iq"]
    return optimal


def plot_iq_sweep(results):
    for bench in {r["benchmark"] for r in results}:
        plt.figure()
        for width in sorted({r["width"] for r in results}):
            subset = [r for r in results if r["benchmark"] == bench and r["width"] == width and r["rob"] == 512]
            subset.sort(key=lambda r: r["iq"])
            plt.plot([r["iq"] for r in subset], [r["ipc"] for r in subset], marker="o", label=f"W={width}")
        plt.xlabel("IQ_SIZE")
        plt.ylabel("IPC")
        plt.title(f"{bench} (ROB=512)")
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        out_path = GRAPH_DIR / f"iq_sweep_{bench}.png"
        plt.savefig(out_path)
        plt.close()
        print(f"Wrote {out_path}")


def plot_rob_sweep(results, optimal_iq):
    for bench in {r["benchmark"] for r in results}:
        plt.figure()
        for width in sorted({r["width"] for r in results}):
            iq_opt = optimal_iq.get(bench, {}).get(width)
            if iq_opt is None:
                continue
            subset = [r for r in results if r["benchmark"] == bench and r["width"] == width and r["iq"] == iq_opt]
            subset.sort(key=lambda r: r["rob"])
            plt.plot([r["rob"] for r in subset], [r["ipc"] for r in subset], marker="o", label=f"W={width} (IQ={iq_opt})")
        plt.xlabel("ROB_SIZE")
        plt.ylabel("IPC")
        plt.title(f"{bench} (Optimized IQ per width)")
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        out_path = GRAPH_DIR / f"rob_sweep_{bench}.png"
        plt.savefig(out_path)
        plt.close()
        print(f"Wrote {out_path}")


def main():
    results = load_results()
    optimal_iq = compute_optimal_iq(results)
    plot_iq_sweep(results)
    plot_rob_sweep(results, optimal_iq)
    print("Optimal IQ per width:")
    for bench, widths in optimal_iq.items():
        for width, iq in sorted(widths.items()):
            print(f"  {bench} width {width}: IQ {iq}")


if __name__ == "__main__":
    main()
