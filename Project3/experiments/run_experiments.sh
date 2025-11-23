#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIM="${ROOT_DIR}/sim"
RESULT_DIR="${ROOT_DIR}/experiments/results"
mkdir -p "${RESULT_DIR}"

if [[ ! -x "${SIM}" ]]; then
  make -C "${ROOT_DIR}"
fi

ROB_SIZES=(32 64 128 256 512)
IQ_SIZES=(8 16 32 64 128 256)
WIDTHS=(1 2 4 8)
TRACES=("proj3-traces/val_trace_gcc1" "proj3-traces/val_trace_perl1")

for trace in "${TRACES[@]}"; do
  base="$(basename "${trace}")"
  for rob in "${ROB_SIZES[@]}"; do
    for iq in "${IQ_SIZES[@]}"; do
      for w in "${WIDTHS[@]}"; do
        out="${RESULT_DIR}/${base}_rob${rob}_iq${iq}_w${w}.txt"
        echo "Running ${trace} ROB=${rob} IQ=${iq} W=${w}"
        "${SIM}" "${rob}" "${iq}" "${w}" "${ROOT_DIR}/${trace}" > "${out}"
      done
    done
  done
done

echo "All experiment runs complete. Raw outputs are in ${RESULT_DIR}"
