# Branch Predictor Simulator (Project 2)

This repository implements the ECE 463/563 Project 2 branch prediction simulator and supporting tooling for validation, sweeps, and report preparation.

## Build

```bash
make
```

This produces the `sim` executable at the repository root.

## Running the Simulator

```
./sim bimodal <M2> <tracefile>
./sim gshare  <M1> <N> <tracefile>
./sim hybrid  <K> <M1> <N> <M2> <tracefile>
```

All traces live under `traces/`. Example invocations should point to those paths explicitly.

Example:

```bash
./sim gshare 9 3 traces/gcc_trace.txt
```

## Validation Suite

```bash
scripts/validate.sh
```

The script rebuilds no artifacts; run `make` first. Each validation output is compared with the official reference using `diff -iw`. A summary line reports the total pass/fail counts and the exit status reflects the overall result.

## Sweeps, CSVs, and Plots

```bash
python3 scripts/sweep.py
```

The sweep driver runs:

- Bimodal sweeps for `m = 7..20` across `gcc`, `jpeg`, and `perl`.
- Gshare sweeps for `m = 7..20` with `n = 0..m` on `gcc`.
- Optional hybrid sweeps (default coverage inspired by the validation runs). Provide a JSON configuration via `--hybrid-config` to explore custom parameter grids.

CSV outputs appear under `graphs/results/` and plots (Matplotlib) under `graphs/figs/`. Use `--no-plots` to skip plotting, and `--bimodal`, `--gshare`, or `--hybrid` to limit execution to specific experiment families. Feel free to create a local virtual environment in `graphs/.venv` for Python package installs.

Matplotlib is required for plot generation. Install it with `pip install matplotlib` if it is not available locally.

### Hybrid Sweep Configuration

Hybrid combinations can be supplied via a JSON file with the following structure:

```json
{
  "trace": "gcc_trace.txt",
  "parameters": [
    {"k": 8, "m1": 14, "n": 10, "m2": 5}
  ]
}
```

Invoke the sweep with:

```bash
python3 scripts/sweep.py --hybrid --hybrid-config path/to/config.json
```

## Repository Layout

```
Makefile              # builds ./sim
sim.cc, sim.h         # simulator sources
traces/               # original trace inputs
validation/
  expected/           # authoritative validation outputs
scripts/
  validate.sh         # validation harness
  sweep.py            # experiment and plotting driver
graphs/
  results/            # generated CSV artifacts (git-ignored)
  figs/               # generated figures (git-ignored)
  .venv/              # optional Python virtual environment (not tracked)
report.pdf            # export of the report template (placeholder until completed)
```

## Notes

- `traces/sample_trace_small.txt` contains the first 50 branches from `traces/gcc_trace.txt` for quick smoke tests.
- Intermediate sweep outputs (`graphs/results/`, `graphs/figs/`) are not tracked in version control; regenerate them as needed.
- Hybrid plotting defaults to a compact scatter but can be adapted for course-specific expectations.
