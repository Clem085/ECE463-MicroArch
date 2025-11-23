#!/usr/bin/env python3
import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RESULT_DIR = ROOT / "results"
OUTPUT_CSV = ROOT / "results.csv"

pattern = re.compile(
    r"(?P<bench>val_trace_\w+)_rob(?P<rob>\d+)_iq(?P<iq>\d+)_w(?P<w>\d+)\.txt"
)

def parse_summary(path: Path):
    bench = rob = iq = width = None
    m = pattern.match(path.name)
    if not m:
        return None
    bench = m.group("bench")
    rob = int(m.group("rob"))
    iq = int(m.group("iq"))
    width = int(m.group("w"))

    cycles = ipc = inst = None
    with path.open() as f:
        for line in f:
            if "Dynamic Instruction Count" in line:
                inst = int(line.strip().split("=")[1])
            elif "Cycles" in line and "Instruction" not in line:
                cycles = int(line.strip().split("=")[1])
            elif "Instructions Per Cycle" in line:
                ipc = float(line.strip().split("=")[1])
    if None in (inst, cycles, ipc):
        return None
    return {
        "benchmark": bench,
        "rob": rob,
        "iq": iq,
        "width": width,
        "inst_count": inst,
        "cycles": cycles,
        "ipc": ipc,
    }


def main():
    rows = []
    for file in RESULT_DIR.glob("*.txt"):
        parsed = parse_summary(file)
        if parsed:
            rows.append(parsed)
    rows.sort(key=lambda r: (r["benchmark"], r["rob"], r["iq"], r["width"]))
    with OUTPUT_CSV.open("w", newline="") as csvfile:
        writer = csv.DictWriter(
            csvfile,
            fieldnames=["benchmark", "rob", "iq", "width", "inst_count", "cycles", "ipc"],
        )
        writer.writeheader()
        writer.writerows(rows)
    print(f"Wrote {len(rows)} rows to {OUTPUT_CSV}")


if __name__ == "__main__":
    main()
