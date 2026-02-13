#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
XASM="$ROOT_DIR/xasm"

if [ ! -x "$XASM" ]; then
    echo "error: $XASM not found; build xasm first" >&2
    exit 1
fi

TMPDIR=$(mktemp -d "${TMPDIR:-/tmp}/xorcyst-regression.XXXXXX")
trap 'rm -rf "$TMPDIR"' EXIT INT TERM

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

run_expect_success() {
    asm_file=$1
    out_file="$TMPDIR/out-success.o"
    log_file="$TMPDIR/out-success.log"

    if ! "$XASM" "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected success for $asm_file"
    fi
}

run_expect_error_no_crash() {
    asm_file=$1
    expected=$2
    out_file="$TMPDIR/out-error.o"
    log_file="$TMPDIR/out-error.log"

    set +e
    "$XASM" "$asm_file" -o "$out_file" >"$log_file" 2>&1
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
        cat "$log_file" >&2
        fail "expected failure for $asm_file"
    fi

    if [ "$status" -ge 128 ]; then
        cat "$log_file" >&2
        fail "xasm crashed for $asm_file (exit $status)"
    fi

    if ! grep -q -- "$expected" "$log_file"; then
        cat "$log_file" >&2
        fail "missing expected error '$expected' for $asm_file"
    fi
}

# Existing sanity tests
run_expect_success "$ROOT_DIR/tests/ifndef.asm"
run_expect_success "$ROOT_DIR/tests/macro.asm"

# Regression: long include path must not smash stack
long_name=$(awk 'BEGIN { for (i = 0; i < 500; ++i) printf "a" }')
printf 'INCSRC "%s"\n' "$long_name" > "$TMPDIR/long-incsrc.asm"
run_expect_error_no_crash "$TMPDIR/long-incsrc.asm" "could not open"

# Regression: long INCBIN path must not smash stack
printf 'INCBIN "%s"\n' "$long_name" > "$TMPDIR/long-incbin.asm"
run_expect_error_no_crash "$TMPDIR/long-incbin.asm" "could not open"

# Regression: >128 unresolved refs to same forward branch must error, not crash
{
    printf 'start:\n'
    i=0
    while [ "$i" -lt 129 ]; do
        printf '    beq ++\n'
        i=$((i + 1))
    done
    printf '++:\n'
    printf '    nop\n'
} > "$TMPDIR/forward-branch-overflow.asm"
run_expect_error_no_crash "$TMPDIR/forward-branch-overflow.asm" "too many unresolved references"

# Regression: single '+'/'-' local branch labels inside PROC must work
cat > "$TMPDIR/proc-single-branch.asm" <<'ASM'
.proc __progbuf_exec
    ldx offset
  - dex
    bmi +
    lda items_hi,x
    pha
    lda items_lo,x
    pha
    jmp -
  + inx
    stx offset
    rts
offset .db 0
items_hi .db 0
items_lo .db 0
.endp
ASM
run_expect_success "$TMPDIR/proc-single-branch.asm"

echo "All regression tests passed"
