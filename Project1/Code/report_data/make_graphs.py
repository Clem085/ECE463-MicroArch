#!/usr/bin/env python3
import os, sys, subprocess, math, json, re, csv, time
from pathlib import Path
from typing import Dict, Any, List, Optional
import yaml
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.ticker import PercentFormatter

HERE = Path(__file__).resolve().parent
OUT = HERE if str(HERE).startswith("/mnt/data") else Path.cwd()

RE = {
    "L1_reads": re.compile(r"L1 reads.*[:=]\s*([0-9]+)", re.I),
    "L1_read_misses": re.compile(r"L1 read misses.*[:=]\s*([0-9]+)", re.I),
    "L1_writes": re.compile(r"L1 writes.*[:=]\s*([0-9]+)", re.I),
    "L1_write_misses": re.compile(r"L1 write misses.*[:=]\s*([0-9]+)", re.I),
    "MRL1": re.compile(r"L1 miss rate.*[:=]\s*([0-9.]+)", re.I),
    "L1_writebacks": re.compile(r"L1 writebacks.*[:=]\s*([0-9]+)", re.I),
    "L2_reads_not_pref": re.compile(r"L2 reads\s*\(demand\).*[:=]\s*([0-9]+)", re.I),
    "L2_read_misses_not_pref": re.compile(r"L2 read misses\s*\(demand\).*[:=]\s*([0-9]+)", re.I),
    "L2_reads_from_pref": re.compile(r"L2 reads\s*\(prefetch\).*[:=]\s*([0-9]+)", re.I),
    "L2_read_misses_from_pref": re.compile(r"L2 read misses\s*\(prefetch\).*[:=]\s*([0-9]+)", re.I),
    "L2_writes": re.compile(r"L2 writes.*[:=]\s*([0-9]+)", re.I),
    "L2_write_misses": re.compile(r"L2 write misses.*[:=]\s*([0-9]+)", re.I),
    "MRL2": re.compile(r"L2 miss rate.*[:=]\s*([0-9.]+)", re.I),
    "L2_writebacks": re.compile(r"L2 writebacks.*[:=]\s*([0-9]+)", re.I),
    "L1_prefetches": re.compile(r"L1 prefetches.*[:=]\s*([0-9]+)", re.I),
    "L2_prefetches": re.compile(r"L2 prefetches.*[:=]\s*([0-9]+)", re.I),
    "total_mem_traffic": re.compile(r"(?:total\s+)?memory traffic.*[:=]\s*([0-9]+)", re.I),
}

RESULT_COLUMNS = [
    "trace","BLOCKSIZE","L1_SIZE","L1_ASSOC","L2_SIZE","L2_ASSOC","PREF_N","PREF_M",
    "L1_reads","L1_read_misses","L1_writes","L1_write_misses","MRL1",
    "L1_writebacks","L2_reads_not_pref","L2_read_misses_not_pref",
    "L2_reads_from_pref","L2_read_misses_from_pref",
    "L2_writes","L2_write_misses","MRL2","L2_writebacks",
    "L1_prefetches","L2_prefetches","total_mem_traffic","AAT_ns"
]

IDENTITY_COLS = {
    "trace","BLOCKSIZE","L1_SIZE","L1_ASSOC","L2_SIZE","L2_ASSOC",
    "PREF_N","PREF_M","TRACE","AAT_ns"
}
METRIC_COLS = set(RESULT_COLUMNS) - IDENTITY_COLS

def parse_output(text: str):
    vals = {}
    for key, rgx in RE.items():
        m = rgx.search(text)
        if m:
            try:
                vals[key] = float(m.group(1))
            except:
                vals[key] = None
    if "MRL1" not in vals and ("L1_reads" in vals and "L1_writes" in vals):
        num = (vals.get("L1_read_misses") or 0) + (vals.get("L1_write_misses") or 0)
        den = (vals.get("L1_reads") or 0) + (vals.get("L1_writes") or 0)
        vals["MRL1"] = (num / den) if den else None
    if "MRL2" not in vals and "L2_reads_not_pref" in vals:
        num = (vals.get("L2_read_misses_not_pref") or 0)
        den = (vals.get("L2_reads_not_pref") or 0)
        vals["MRL2"] = (num / den) if den else None
    return vals

def aat_no_l2(L1_reads, L1_writes, HTL1, MissPenalty, L1_read_misses, L1_write_misses):
    den = (L1_reads + L1_writes) if (L1_reads is not None and L1_writes is not None) else None
    if den in (None, 0):
        return None
    miss_total = (L1_read_misses or 0) + (L1_write_misses or 0)
    return HTL1 + MissPenalty * (miss_total / den)

def aat_with_l2(L1_reads, L1_writes, HTL1, HTL2, MissPenalty,
                L1_read_misses, L1_write_misses,
                L2_reads_not_pref, L2_read_misses_not_pref):
    den = (L1_reads + L1_writes) if (L1_reads is not None and L1_writes is not None) else None
    if den in (None, 0):
        return None
    miss_L1_total = (L1_read_misses or 0) + (L1_write_misses or 0)
    term_L2 = (miss_L1_total / den) * HTL2
    term_MEM = ((L2_read_misses_not_pref or 0) / den) * MissPenalty
    return HTL1 + term_L2 + term_MEM

def run_sim_once(sim_path: Path, args):
    proc = subprocess.run([str(sim_path)] + list(args), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return proc.stdout

def ensure_assoc(sizeB: int, blocksize: int, assoc):
    if assoc == "FA":
        return sizeB // blocksize
    return int(assoc)

def make_args(order, cfg):
    mapping = {
        "BLOCKSIZE": str(cfg["BLOCKSIZE"]),
        "L1_SIZE": str(cfg["L1_SIZE"]),
        "L1_ASSOC": str(cfg["L1_ASSOC"]),
        "L2_SIZE": str(cfg["L2_SIZE"]),
        "L2_ASSOC": str(cfg["L2_ASSOC"]),
        "PREF_N": str(cfg["PREF_N"]),
        "PREF_M": str(cfg["PREF_M"]),
        "TRACE": cfg["TRACE"],
    }
    return [mapping[key] for key in order]

import math
def grid_graph1(config, sim_path, arg_order, cacti, trace):
    blk = int(config["blocksize"])
    assoc_list = config["l1_assoc_list"]
    results = []
    for size_kb in config["l1_sizes_kb"]:
        sizeB = int(size_kb) * 1024
        for assoc in assoc_list:
            l1_assoc = ensure_assoc(sizeB, blk, assoc)
            run_cfg = dict(BLOCKSIZE=blk, L1_SIZE=sizeB, L1_ASSOC=l1_assoc,
                           L2_SIZE=0, L2_ASSOC=0, PREF_N=config["prefetch"][0],
                           PREF_M=config["prefetch"][1], TRACE=trace)
            args = make_args(arg_order, run_cfg)
            rec = {k: None for k in RESULT_COLUMNS}
            rec.update(dict(trace=trace, **run_cfg))
            if sim_path.is_file() and os.access(sim_path, os.X_OK):
                out = run_sim_once(sim_path, args)
                vals = parse_output(out)
                for k, v in vals.items():
                    if k in METRIC_COLS:
                        rec[k] = v
            rec["AAT_ns"] = aat_no_l2(rec["L1_reads"], rec["L1_writes"],
                                      cacti["HTL1_ns"], cacti["Miss_Penalty_ns"],
                                      rec["L1_read_misses"], rec["L1_write_misses"])
            results.append(rec)
    return pd.DataFrame(results)

def grid_graph2(config, sim_path, arg_order, cacti, trace):
    return grid_graph1(config, sim_path, arg_order, cacti, trace)

def grid_graph3(config, sim_path, arg_order, cacti, trace):
    blk = int(config["blocksize"])
    results = []
    for size_kb in config["l1_sizes_kb"]:
        sizeB = int(size_kb) * 1024
        for assoc in config["l1_assoc_list"]:
            l1_assoc = ensure_assoc(sizeB, blk, assoc)
            run_cfg = dict(BLOCKSIZE=blk, L1_SIZE=sizeB, L1_ASSOC=l1_assoc,
                           L2_SIZE=int(config["l2_size_kb"]) * 1024, L2_ASSOC=int(config["l2_assoc"]),
                           PREF_N=config["prefetch"][0], PREF_M=config["prefetch"][1],
                           TRACE=trace)
            args = make_args(arg_order, run_cfg)
            rec = {k: None for k in RESULT_COLUMNS}
            rec.update(dict(trace=trace, **run_cfg))
            if sim_path.is_file() and os.access(sim_path, os.X_OK):
                out = run_sim_once(sim_path, args)
                vals = parse_output(out)
                for k, v in vals.items():
                    if k in METRIC_COLS:
                        rec[k] = v
            rec["AAT_ns"] = aat_with_l2(rec["L1_reads"], rec["L1_writes"],
                                        cacti["HTL1_ns"], cacti["HTL2_ns"], cacti["Miss_Penalty_ns"],
                                        rec["L1_read_misses"], rec["L1_write_misses"],
                                        rec["L2_reads_not_pref"], rec["L2_read_misses_not_pref"])
            results.append(rec)
    return pd.DataFrame(results)

def grid_graph4(config, sim_path, arg_order, cacti, trace):
    results = []
    for blk in config["blocksizes"]:
        for size_kb in config["l1_sizes_kb"]:
            sizeB = int(size_kb) * 1024
            l1_assoc = int(config["l1_assoc"])
            run_cfg = dict(BLOCKSIZE=int(blk), L1_SIZE=sizeB, L1_ASSOC=l1_assoc,
                           L2_SIZE=0, L2_ASSOC=0, PREF_N=config["prefetch"][0],
                           PREF_M=config["prefetch"][1], TRACE=trace)
            args = make_args(arg_order, run_cfg)
            rec = {k: None for k in RESULT_COLUMNS}
            rec.update(dict(trace=trace, **run_cfg))
            if sim_path.is_file() and os.access(sim_path, os.X_OK):
                out = run_sim_once(sim_path, args)
                vals = parse_output(out)
                for k, v in vals.items():
                    if k in METRIC_COLS:
                        rec[k] = v
            rec["AAT_ns"] = aat_no_l2(rec["L1_reads"], rec["L1_writes"],
                                      cacti["HTL1_ns"], cacti["Miss_Penalty_ns"],
                                      rec["L1_read_misses"], rec["L1_write_misses"])
            results.append(rec)
    return pd.DataFrame(results)

def grid_graph5(config, sim_path, arg_order, cacti, trace):
    blk = int(config["blocksize"])
    results = []
    for l2_kb in config["l2_sizes_kb"]:
        for size_kb in config["l1_sizes_kb"]:
            sizeB = int(size_kb) * 1024
            run_cfg = dict(BLOCKSIZE=blk, L1_SIZE=sizeB, L1_ASSOC=int(config["l1_assoc"]),
                           L2_SIZE=int(l2_kb) * 1024, L2_ASSOC=int(config["l2_assoc"]),
                           PREF_N=config["prefetch"][0], PREF_M=config["prefetch"][1],
                           TRACE=trace)
            args = make_args(arg_order, run_cfg)
            rec = {k: None for k in RESULT_COLUMNS}
            rec.update(dict(trace=trace, **run_cfg))
            if sim_path.is_file() and os.access(sim_path, os.X_OK):
                out = run_sim_once(sim_path, args)
                vals = parse_output(out)
                for k, v in vals.items():
                    if k in METRIC_COLS:
                        rec[k] = v
            rec["AAT_ns"] = aat_with_l2(rec["L1_reads"], rec["L1_writes"],
                                        cacti["HTL1_ns"], cacti["HTL2_ns"], cacti["Miss_Penalty_ns"],
                                        rec["L1_read_misses"], rec["L1_write_misses"],
                                        rec["L2_reads_not_pref"], rec["L2_read_misses_not_pref"])
            results.append(rec)
    return pd.DataFrame(results)

def plot_graph1(df, outdir):
    df = df.copy()
    df["log2_L1_SIZE"] = df["L1_SIZE"].apply(lambda x: math.log2(x) if x and x > 0 else None)
    def label_assoc(row):
        return "FA" if row["L1_ASSOC"] == (row["L1_SIZE"] / row["BLOCKSIZE"]) else int(row["L1_ASSOC"])
    df["assoc_label"] = df.apply(label_assoc, axis=1)
    plt.figure()
    for label, g in df.groupby("assoc_label"):
        g = g.sort_values("log2_L1_SIZE")
        plt.plot(g["log2_L1_SIZE"], g["MRL1"], marker="o", label=str(label))
    plt.xlabel("log2(L1 SIZE in bytes)")
    ax = plt.gca()
    ax.yaxis.set_major_formatter(PercentFormatter(xmax=1, decimals=2))
    plt.ylabel("L1 miss rate (%)")
    plt.title("Graph #1: L1 miss rate vs log2(L1 SIZE) by associativity")
    plt.legend(title="Assoc")
    plt.grid(True, which="both", linestyle=":")
    plt.tight_layout()
    out = outdir / "graph1.png"
    plt.savefig(out, dpi=200)
    plt.close()

def plot_graph2(df, outdir):
    df = df.copy()
    df["log2_L1_SIZE"] = df["L1_SIZE"].apply(lambda x: math.log2(x) if x and x > 0 else None)
    plt.figure()
    for label, g in df.groupby("L1_ASSOC"):
        g = g.sort_values("log2_L1_SIZE")
        plt.plot(g["log2_L1_SIZE"], g["AAT_ns"], marker="o", label=f"{int(label)}-way")
    plt.xlabel("log2(L1 SIZE in bytes)")
    plt.ylabel("AAT (ns)")
    plt.title("Graph #2: AAT vs log2(L1 SIZE), Given L1 Associativity")
    plt.legend(title="L1 Assoc")
    plt.grid(True, which="both", linestyle=":")
    plt.tight_layout()
    out = outdir / "graph2.png"
    plt.savefig(out, dpi=200)
    plt.close()

def plot_graph3(df, outdir):
    df = df.copy()
    df["log2_L1_SIZE"] = df["L1_SIZE"].apply(lambda x: math.log2(x) if x and x > 0 else None)
    plt.figure()
    for label, g in df.groupby("L1_ASSOC"):
        g = g.sort_values("log2_L1_SIZE")
        plt.plot(g["log2_L1_SIZE"], g["AAT_ns"], marker="o", label=f"{int(label)}-way")
    plt.xlabel("log2(L1 SIZE in bytes)")
    plt.ylabel("AAT (ns)")
    plt.title("Graph #3: AAT vs log2(L1 SIZE) with L2=16KB 8-way")
    plt.legend(title="L1 Assoc")
    plt.grid(True, which="both", linestyle=":")
    plt.tight_layout()
    out = outdir / "graph3.png"
    plt.savefig(out, dpi=200)
    plt.close()

def plot_graph4(df, outdir):
    df = df.copy()
    df["log2_BLOCKSIZE"] = df["BLOCKSIZE"].apply(lambda x: math.log2(x) if x and x > 0 else None)
    plt.figure()
    for sizeB, g in df.groupby("L1_SIZE"):
        g = g.sort_values("log2_BLOCKSIZE")
        lbl = f"{int(sizeB/1024)}KB"
        plt.plot(g["log2_BLOCKSIZE"], g["MRL1"], marker="o", label=lbl)
    plt.xlabel("log2(BLOCKSIZE in bytes)")
    ax = plt.gca()
    ax.yaxis.set_major_formatter(PercentFormatter(xmax=1, decimals=2))
    plt.ylabel("L1 miss rate (%)")
    plt.title("Graph #4: L1 miss rate vs log2(BLOCKSIZE) (L1 4-way)")
    plt.legend(title="L1 Size", loc="upper left")
    plt.grid(True, which="both", linestyle=":")
    plt.tight_layout()
    out = outdir / "graph4.png"
    plt.savefig(out, dpi=200)
    plt.close()

def plot_graph5(df, outdir):
    df = df.copy()
    df["log2_L1_SIZE"] = df["L1_SIZE"].apply(lambda x: math.log2(x) if x and x > 0 else None)
    plt.figure()
    for l2size, g in df.groupby("L2_SIZE"):
        g = g.sort_values("log2_L1_SIZE")
        lbl = f"L2={int(l2size/1024)}KB"
        plt.plot(g["log2_L1_SIZE"], g["AAT_ns"], marker="o", label=lbl)
    plt.xlabel("log2(L1 SIZE in bytes)")
    plt.ylabel("AAT (ns)")
    plt.title("Graph #5: AAT vs log2(L1 SIZE) by L2 SIZE (L1 4-way, L2 8-way)")
    plt.legend(title="Config")
    plt.grid(True, which="both", linestyle=":")
    plt.tight_layout()
    out = outdir / "graph5.png"
    plt.savefig(out, dpi=200)
    plt.close()

def maybe_concat(acc, nxt):
    if acc is None:
        return nxt
    return pd.concat([acc, nxt], ignore_index=True)

def main():
    cfg_path = HERE / "microarch_config.yaml"
    if not cfg_path.exists():
        print(f"Missing config: {cfg_path}")
        sys.exit(1)
    cfg = yaml.safe_load(cfg_path.read_text())

    sim_path = Path(cfg.get("sim_path", "sim")).expanduser().resolve()
    traces = cfg.get("traces", [])
    arg_order = cfg.get("arg_order", ["BLOCKSIZE","L1_SIZE","L1_ASSOC","L2_SIZE","L2_ASSOC","PREF_N","PREF_M","TRACE"])
    cacti = cfg.get("cacti_params", {"HTL1_ns":1.0,"HTL2_ns":8.0,"Miss_Penalty_ns":100.0})
    expt_flags = cfg.get("experiments", {})
    outdir = OUT

    precomp_csv = cfg.get("precomputed_results_csv", "")
    if precomp_csv:
        df_all = pd.read_csv(precomp_csv)
        print(f"Loaded precomputed results: {precomp_csv} ({len(df_all)} rows)")
    else:
        if not sim_path.exists():
            print(f"Notice: sim binary not found at {sim_path}. I will still build the plotting scaffolding.")
        if not traces:
            print("Notice: no traces listed. Add at least one trace path in microarch_config.yaml.")

        df_all = None
        for trace in traces:
            if not Path(trace).exists():
                print(f"Trace not found (skipping): {trace}")
                continue
            if expt_flags.get("graph1", True):
                df = grid_graph1(cfg["graph1"], sim_path, arg_order, cacti, trace)
                df_all = maybe_concat(df_all, df)
            if expt_flags.get("graph2", True):
                df = grid_graph2(cfg["graph2"], sim_path, arg_order, cacti, trace)
                df_all = maybe_concat(df_all, df)
            if expt_flags.get("graph3", True):
                df = grid_graph3(cfg["graph3"], sim_path, arg_order, cacti, trace)
                df_all = maybe_concat(df_all, df)
            if expt_flags.get("graph4", True):
                df = grid_graph4(cfg["graph4"], sim_path, arg_order, cacti, trace)
                df_all = maybe_concat(df_all, df)
            if expt_flags.get("graph5", True):
                df = grid_graph5(cfg["graph5"], sim_path, arg_order, cacti, trace)
                df_all = maybe_concat(df_all, df)

        if df_all is None:
            df_all = pd.DataFrame(columns=RESULT_COLUMNS)

    num_cols = [
        "BLOCKSIZE","L1_SIZE","L1_ASSOC","L2_SIZE","L2_ASSOC",
        "PREF_N","PREF_M","MRL1","MRL2","AAT_ns",
        "L1_reads","L1_read_misses","L1_writes","L1_write_misses",
        "L1_writebacks","L2_reads_not_pref","L2_read_misses_not_pref",
        "L2_reads_from_pref","L2_read_misses_from_pref",
        "L2_writes","L2_write_misses","L2_writebacks",
        "L1_prefetches","L2_prefetches","total_mem_traffic"
    ]
    for col in num_cols:
        if col in df_all.columns:
            df_all[col] = pd.to_numeric(df_all[col], errors="coerce")

    results_csv = outdir / "all_results.csv"
    df_all.to_csv(results_csv, index=False)
    print(f"Wrote results: {results_csv} ({len(df_all)} rows)")

    made_any_plot = False
    g1 = df_all[(df_all["L2_SIZE"] == 0) & (df_all["BLOCKSIZE"] == cfg["graph1"]["blocksize"])]
    if not g1.empty:
        plot_graph1(g1, outdir); made_any_plot = True

    g2 = df_all[(df_all["L2_SIZE"] == 0) & (df_all["BLOCKSIZE"] == cfg["graph2"]["blocksize"]) & (df_all["L1_ASSOC"] <= 8)]
    if not g2.empty:
        plot_graph2(g2, outdir); made_any_plot = True

    g3 = df_all[(df_all["L2_SIZE"] == cfg["graph3"]["l2_size_kb"] * 1024) & (df_all["BLOCKSIZE"] == cfg["graph3"]["blocksize"])]
    if not g3.empty:
        plot_graph3(g3, outdir); made_any_plot = True

    g4 = df_all[(df_all["L2_SIZE"] == 0) & (df_all["L1_ASSOC"] == cfg["graph4"]["l1_assoc"]) & (df_all["L1_SIZE"].isin([kb*1024 for kb in cfg["graph4"]["l1_sizes_kb"]]))]
    if not g4.empty:
        plot_graph4(g4, outdir); made_any_plot = True

    g5 = df_all[(df_all["BLOCKSIZE"] == cfg["graph5"]["blocksize"]) & (df_all["L1_ASSOC"] == cfg["graph5"]["l1_assoc"]) & (df_all["L2_ASSOC"] == cfg["graph5"]["l2_assoc"]) & (df_all["L2_SIZE"].isin([kb*1024 for kb in cfg["graph5"]["l2_sizes_kb"]]))]
    if not g5.empty:
        plot_graph5(g5, outdir); made_any_plot = True

    if made_any_plot:
        print("Saved figures to:")
        for i in range(1,6):
            p = outdir / f"graph{i}.png"
            if p.exists():
                print(f" - {p}")
    else:
        print("No plots were created yet. Populate traces, compile sim, or point to a precomputed CSV, then re-run.")

if __name__ == "__main__":
    main()
