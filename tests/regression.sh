#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
XASM="$ROOT_DIR/xasm"
XLNK="$ROOT_DIR/xlnk"
cd "$ROOT_DIR"

if [ ! -x "$XASM" ]; then
    echo "error: $XASM not found; build xasm first" >&2
    exit 1
fi

if [ ! -x "$XLNK" ]; then
    echo "error: $XLNK not found; build xlnk first" >&2
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

run_expect_success_pure_binary() {
    asm_file=$1
    out_file="$TMPDIR/out-pure.bin"
    log_file="$TMPDIR/out-pure.log"

    if ! "$XASM" --pure-binary "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected pure-binary success for $asm_file"
    fi
}

run_expect_success_with_listing() {
    asm_file=$1
    out_file="$TMPDIR/out-listing.o"
    lst_file="$TMPDIR/out-listing.lst"
    log_file="$TMPDIR/out-listing.log"

    if ! "$XASM" --lst="$lst_file" "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected listing success for $asm_file"
    fi

    if [ ! -s "$lst_file" ]; then
        fail "listing file was not generated for $asm_file"
    fi

    if ! grep -q "LINE  ADDR  HEX BYTES     SOURCE CODE" "$lst_file"; then
        fail "listing header missing for $asm_file"
    fi

    if ! grep -q "SYMBOL TABLE:" "$lst_file"; then
        fail "symbol table section missing for $asm_file"
    fi

    if ! grep -Eq "^[0-9]{4}  [0-9A-F]{4}  " "$lst_file"; then
        fail "listing body missing address rows for $asm_file"
    fi
}

run_expect_success_pure_binary_with_listing() {
    asm_file=$1
    out_file="$TMPDIR/out-pure-listing.bin"
    lst_file="$TMPDIR/out-pure-listing.lst"
    log_file="$TMPDIR/out-pure-listing.log"

    if ! "$XASM" --pure-binary --listing="$lst_file" "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected pure-binary listing success for $asm_file"
    fi

    if [ ! -s "$lst_file" ]; then
        fail "pure-binary listing file was not generated for $asm_file"
    fi

    if ! grep -q "8000" "$lst_file"; then
        fail "expected ORG address missing from pure-binary listing for $asm_file"
    fi

    if ! grep -q "A9 01" "$lst_file"; then
        fail "expected opcode bytes missing from pure-binary listing for $asm_file"
    fi

    if ! grep -q "SYMBOL TABLE:" "$lst_file"; then
        fail "symbol table section missing from pure-binary listing for $asm_file"
    fi
}

run_expect_scoped_label_listing() {
    asm_file="$TMPDIR/listing-label-scope.asm"
    out_file="$TMPDIR/listing-label-scope.bin"
    lst_file="$TMPDIR/listing-label-scope.lst"
    log_file="$TMPDIR/listing-label-scope.log"

    cat > "$asm_file" <<'ASM'
.org $c000
Reset:
ldx #1
- dex
  bpl -
- dex
  bpl -
  rts
@@loop:
  dey
  bpl @@loop
Reset2:
  ldx #1
  @@loop:
  dex
  bpl @@loop
.end
ASM

    if ! "$XASM" --pure-binary --listing="$lst_file" "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected scoped-label listing success"
    fi

    if ! grep -Eq '^[0-9]{4}  C000[[:space:]]+Reset:' "$lst_file"; then
        fail "expected Reset label line in listing"
    fi

    if ! grep -Eq '^[0-9]{4}  C00C[[:space:]]+Reset2:' "$lst_file"; then
        fail "expected Reset2 label line in listing"
    fi

    if ! grep -Eq '^Reset-#0[[:space:]]+= \$C002$' "$lst_file"; then
        fail "expected scoped backward label Reset-#0 in symbol table"
    fi

    if ! grep -Eq '^Reset-#1[[:space:]]+= \$C005$' "$lst_file"; then
        fail "expected scoped backward label Reset-#1 in symbol table"
    fi

    if ! grep -Eq '^Reset@@loop[[:space:]]+= \$C009$' "$lst_file"; then
        fail "expected scoped local label Reset@@loop in symbol table"
    fi

    if ! grep -Eq '^Reset2@@loop[[:space:]]+= \$C00E$' "$lst_file"; then
        fail "expected scoped local label Reset2@@loop in symbol table"
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

run_xlnk_expect_success() {
    script_file=$1
    log_file="$TMPDIR/xlnk-success.log"

    if ! "$XLNK" "$script_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected linker success for $script_file"
    fi
}

run_xlnk_expect_error_no_crash() {
    script_file=$1
    expected=$2
    log_file="$TMPDIR/xlnk-error.log"

    set +e
    "$XLNK" "$script_file" >"$log_file" 2>&1
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
        cat "$log_file" >&2
        fail "expected linker failure for $script_file"
    fi

    if [ "$status" -ge 128 ]; then
        cat "$log_file" >&2
        fail "xlnk crashed for $script_file (exit $status)"
    fi

    if ! grep -q -- "$expected" "$log_file"; then
        cat "$log_file" >&2
        fail "missing expected linker error '$expected' for $script_file"
    fi
}

# Existing sanity tests
run_expect_success "$ROOT_DIR/tests/ifndef.asm"
run_expect_success "$ROOT_DIR/tests/macro.asm"
run_expect_success "$ROOT_DIR/tests/coverage_instructions.asm"
run_expect_success "$ROOT_DIR/tests/coverage_directives_control.asm"
run_expect_success "$ROOT_DIR/tests/coverage_directives_macros_io.asm"
run_expect_success "$ROOT_DIR/tests/coverage_directives_types_symbols.asm"
run_expect_success "$ROOT_DIR/tests/coverage_directives_layout.asm"
run_expect_success_with_listing "$ROOT_DIR/tests/coverage_instructions.asm"
run_expect_success_pure_binary "$ROOT_DIR/tests/coverage_org_pure.asm"
run_expect_success_pure_binary_with_listing "$ROOT_DIR/tests/coverage_org_pure.asm"
run_expect_scoped_label_listing

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

# Regression: ERROR directive should fail cleanly with expected diagnostic
run_expect_error_no_crash "$ROOT_DIR/tests/coverage_error_directive.asm" "intentional regression error"

# Linker regression: simple link script with copy/bank/pad/link should succeed
cat > "$TMPDIR/link_unit_a.asm" <<'ASM'
start:
    lda #$01
    rts
END
ASM
cat > "$TMPDIR/link_unit_b.asm" <<'ASM'
func:
    ldx #$05
    rts
END
ASM

if ! "$XASM" "$TMPDIR/link_unit_a.asm" -o "$TMPDIR/link_unit_a.o" >/dev/null 2>&1; then
    fail "failed to build linker fixture unit A"
fi
if ! "$XASM" "$TMPDIR/link_unit_b.asm" -o "$TMPDIR/link_unit_b.o" >/dev/null 2>&1; then
    fail "failed to build linker fixture unit B"
fi

printf '\xAA\xBB' > "$TMPDIR/link_header.bin"

cat > "$TMPDIR/link_ok.script" <<EOF
output{file=$TMPDIR/link_ok.bin}
copy{file=$TMPDIR/link_header.bin}
bank{size=\$0010,origin=\$8000}
link{file=$TMPDIR/link_unit_a.o}
pad{offset=\$0008}
link{file=$TMPDIR/link_unit_b.o}
EOF

run_xlnk_expect_success "$TMPDIR/link_ok.script"

if [ ! -f "$TMPDIR/link_ok.bin" ]; then
    fail "linker did not produce output file"
fi

out_size=$(wc -c < "$TMPDIR/link_ok.bin" | tr -d '[:space:]')
if [ "$out_size" != "18" ]; then
    fail "unexpected linker output size ($out_size, expected 18)"
fi

# Linker regression: linking before opening output should fail cleanly
cat > "$TMPDIR/link_fail_no_output.script" <<EOF
link{file=$TMPDIR/link_unit_a.o,origin=\$8000}
EOF
run_xlnk_expect_error_no_crash "$TMPDIR/link_fail_no_output.script" "no output open"

echo "All regression tests passed"
