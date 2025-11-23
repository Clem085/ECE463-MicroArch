#!/usr/bin/env python3
import csv
from pathlib import Path
from collections import defaultdict

ROOT = Path(__file__).resolve().parent
CSV_PATH = ROOT / "results.csv"

def load_results():
    with CSV_PATH.open() as f:
        reader = csv.DictReader(f)
        for row in reader:
            row["benchmark"] = row["benchmark"]
            row["width"] = int(row["width"])
            row["rob"] = int(row["rob"])
            row["iq"] = int(row["iq"])
            row["ipc"] = float(row["ipc"])
            yield row

def compute_optimal_iq(results):
    optimal = defaultdict(dict)
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

def main():
    results = list(load_results())
    optimal = compute_optimal_iq(results)
    benches = sorted(optimal.keys())
    widths = [1,2,4,8]
    print("| Benchmark | " + " | ".join(f"W={w}" for w in widths) + " |")
    print("|-----------|" + "|".join(["----"]*len(widths)) + "|")
    for bench in benches:
        row = [bench]
        for w in widths:
            row.append(str(optimal.get(bench, {}).get(w, "-")))
        print("| " + " | ".join(row) + " |")

if __name__ == "__main__":
    main()
