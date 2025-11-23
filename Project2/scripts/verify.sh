#!/usr/bin/env bash

set -euo pipefail

TRACE_DIR=$(pwd)
SIM="./sim"

if [[ ! -x "$SIM" ]]; then
    echo "Error: $SIM not found. Run 'make' first." >&2
    exit 1
fi

tests=(
    "bimodal 6 gcc_trace.txt"
    "bimodal 12 gcc_trace.txt"
    "bimodal 4 jpeg_trace.txt"
    "bimodal 5 perl_trace.txt"
    "gshare 9 3 gcc_trace.txt"
    "gshare 14 8 gcc_trace.txt"
    "gshare 11 5 jpeg_trace.txt"
    "gshare 10 6 perl_trace.txt"
)

all_passed=true

for test in "${tests[@]}"; do
    cmd="$SIM $test"
    echo "Running: $cmd"
    if ! eval "$cmd" >/dev/null; then
        echo "  FAILED (non-zero exit)"
        all_passed=false
    else
        echo "  OK"
    fi
done

if $all_passed; then
    echo "All local checks passed (note: this script does not diff outputs)."
else
    echo "One or more checks failed."
    exit 1
fi
