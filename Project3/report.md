# ECE 463 Project 3 Report (Draft)

**Student:** *<Your Name>*  
**Course:** ECE 463  
**Honor Pledge:** “I have neither given nor received unauthorized aid on this project.”

## Validation Status
- Simulator validated against provided `val1.txt` when invoked as `./sim 16 8 1 val_trace_gcc1`.
- `tests/run_tests.sh` passes for all synthetic traces.
- Experiments scripts prepared for GCC and Perl traces.

## A. Large ROB, Effect of IQ_SIZE (ROB_SIZE = 512)

- Run `experiments/run_experiments.sh` then `experiments/parse_results.py` to populate `experiments/results.csv`.
- Generate graphs: `experiments/plot_graphs.py` (outputs to `experiments/graphs/`).

Place graphs here:
- GCC: `experiments/graphs/iq_sweep_val_trace_gcc1.png`
- Perl: `experiments/graphs/iq_sweep_val_trace_perl1.png`

### Optimized IQ_SIZE per WIDTH (within 6% of IQ=256 IPC)

Generate table with `python experiments/fill_report_table.py` after creating `results.csv`.

| Benchmark | W=1 | W=2 | W=4 | W=8 |
|-----------|----|----|----|----|
| val_trace_gcc1 | *auto-fill* | *auto-fill* | *auto-fill* | *auto-fill* |
| val_trace_perl1 | *auto-fill* | *auto-fill* | *auto-fill* | *auto-fill* |

### Discussion (Draft)
- As WIDTH increases, IQ must grow to expose enough independent instructions; diminishing returns appear once IQ passes the optimized size above.
- Perl vs GCC: if Perl shows larger optimized IQ at higher WIDTH, it suggests denser dependencies or more long-latency ops in Perl traces.

## B. Effect of ROB_SIZE (using optimized IQ per WIDTH)

- Use `experiments/plot_graphs.py` to generate ROB sweep graphs with optimized IQs.

Place graphs here:
- GCC: `experiments/graphs/rob_sweep_val_trace_gcc1.png`
- Perl: `experiments/graphs/rob_sweep_val_trace_perl1.png`

### Discussion (Draft)
- IPC scales with ROB until instruction window captures enough parallelism; beyond that point returns flatten.
- Wider widths rely more on large ROBs to find ready work; narrow widths benefit less.

## How to Regenerate
1. `./tests/run_tests.sh` (quick correctness sanity).
2. `./experiments/run_experiments.sh`
3. `./experiments/parse_results.py`
4. `./experiments/plot_graphs.py`
5. `./experiments/fill_report_table.py` (copy table values above).

## Notes
- All scripts assume traces at `proj3-traces/val_trace_gcc1` and `proj3-traces/val_trace_perl1`.
- Graphs and tables should be updated after rerunning experiments on the final simulator build.
