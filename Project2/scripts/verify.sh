#!/usr/bin/env bash

set -u
set -o pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIM_BIN="$ROOT_DIR/sim"
EXPECTED_DIR="$ROOT_DIR/validation/expected"
TMP_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

if [[ ! -x "$SIM_BIN" ]]; then
    echo "Error: simulator binary not found at $SIM_BIN. Run 'make' first." >&2
    exit 1
fi

if [[ ! -d "$EXPECTED_DIR" ]]; then
    echo "Error: expected outputs directory not found at $EXPECTED_DIR" >&2
    exit 1
fi

pass_count=0
fail_count=0

for expected in "$EXPECTED_DIR"/val_*.txt; do
    [[ -e "$expected" ]] || continue
    cmd_line="$(sed -n '2p' "$expected")"
    cmd_line="${cmd_line#" "}"
    read -r -a args <<< "$cmd_line"

    output_file="$TMP_DIR/$(basename "$expected")"
    if (cd "$ROOT_DIR" && "${args[@]}" > "$output_file"); then
        if diff -iw "$output_file" "$expected" > /dev/null; then
            echo "PASS $(basename "$expected")"
            pass_count=$((pass_count + 1))
        else
            echo "FAIL $(basename "$expected")"
            diff -iw "$output_file" "$expected" || true
            fail_count=$((fail_count + 1))
        fi
    else
        echo "ERROR running ${cmd_line}" >&2
        fail_count=$((fail_count + 1))
    fi
done

echo "Summary: ${pass_count} passed, ${fail_count} failed."

if [[ $fail_count -ne 0 ]]; then
    exit 1
fi
