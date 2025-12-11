#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIM="${ROOT_DIR}/sim"

if [[ ! -x "${SIM}" ]]; then
  make -C "${ROOT_DIR}"
fi

declare -a TRACES=("trace_chain" "trace_parallel" "trace_mix")
FAIL=0

for t in "${TRACES[@]}"; do
  TRACE="tests/${t}.txt"
  EXPECT="${ROOT_DIR}/tests/${t}.expected"
  OUT="${ROOT_DIR}/tests/${t}.out"

  (cd "${ROOT_DIR}" && ./sim 8 4 2 "${TRACE}") > "${OUT}"
  if diff -iw "${OUT}" "${EXPECT}" >/dev/null; then
    echo "[PASS] ${t}"
    rm -f "${OUT}"
  else
    echo "[FAIL] ${t}"
    FAIL=1
  fi
done

exit ${FAIL}
