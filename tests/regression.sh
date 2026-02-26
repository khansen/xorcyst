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

run_expect_success_pure_binary_with_listing_json() {
    asm_file=$1
    out_file="$TMPDIR/out-pure-listing-json.bin"
    lst_file="$TMPDIR/out-pure-listing.json"
    log_file="$TMPDIR/out-pure-listing-json.log"

    if ! "$XASM" --pure-binary --listing="$lst_file" --listing-format=json "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected pure-binary JSON listing success for $asm_file"
    fi

    if [ ! -s "$lst_file" ]; then
        fail "pure-binary JSON listing file was not generated for $asm_file"
    fi

    if ! grep -q '"version": "1"' "$lst_file"; then
        fail "JSON listing version missing for $asm_file"
    fi

    if ! grep -q '"records": \[' "$lst_file"; then
        fail "JSON listing records array missing for $asm_file"
    fi

    if ! grep -q '"directive_or_opcode"' "$lst_file"; then
        fail "JSON listing records missing directive_or_opcode for $asm_file"
    fi

    if ! grep -q '"continuation_of_record":' "$lst_file"; then
        fail "JSON listing records missing continuation_of_record for $asm_file"
    fi
}

run_expect_success_pure_binary_with_listing_ndjson() {
    asm_file=$1
    out_file="$TMPDIR/out-pure-listing-ndjson.bin"
    lst_file="$TMPDIR/out-pure-listing.ndjson"
    log_file="$TMPDIR/out-pure-listing-ndjson.log"

    if ! "$XASM" --pure-binary --listing="$lst_file" --listing-format=ndjson "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected pure-binary NDJSON listing success for $asm_file"
    fi

    if [ ! -s "$lst_file" ]; then
        fail "pure-binary NDJSON listing file was not generated for $asm_file"
    fi

    if ! head -n 1 "$lst_file" | grep -q '"file"'; then
        fail "NDJSON listing first record missing file field for $asm_file"
    fi

    if ! grep -q '"directive_or_opcode"' "$lst_file"; then
        fail "NDJSON listing records missing directive_or_opcode for $asm_file"
    fi
}

run_expect_compare_match() {
    asm_file=$1
    out_file="$TMPDIR/compare-match.bin"
    ref_file="$TMPDIR/compare-match-ref.bin"
    log_file="$TMPDIR/compare-match.log"

    if ! "$XASM" --pure-binary "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "failed to build compare fixture for $asm_file"
    fi
    cp "$out_file" "$ref_file"

    if ! "$XASM" --pure-binary --compare="$ref_file" "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected compare match for $asm_file"
    fi
}

run_expect_compare_mismatch() {
    asm_file=$1
    out_file="$TMPDIR/compare-mismatch.bin"
    ref_file="$TMPDIR/compare-mismatch-ref.bin"
    log_file="$TMPDIR/compare-mismatch.log"
    json_log_file="$TMPDIR/compare-mismatch-json.log"

    if ! "$XASM" --pure-binary "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "failed to build compare mismatch fixture for $asm_file"
    fi
    cp "$out_file" "$ref_file"
    printf '\x00' | dd of="$ref_file" bs=1 seek=0 conv=notrunc >/dev/null 2>&1

    set +e
    "$XASM" --pure-binary --compare="$ref_file" --compare-max-mismatches=1 "$asm_file" -o "$out_file" >"$log_file" 2>&1
    status=$?
    set -e
    if [ "$status" -ne 5 ]; then
        cat "$log_file" >&2
        fail "expected compare mismatch exit code 5 for $asm_file"
    fi
    if ! grep -q 'mismatch #1 at output+' "$log_file"; then
        cat "$log_file" >&2
        fail "expected compare mismatch text output for $asm_file"
    fi
    if ! grep -q 'source:' "$log_file"; then
        cat "$log_file" >&2
        fail "expected compare mismatch source mapping output for $asm_file"
    fi

    set +e
    "$XASM" --pure-binary --compare="$ref_file" --compare-max-mismatches=1 --compare-format=json "$asm_file" -o "$out_file" >"$json_log_file" 2>&1
    status=$?
    set -e
    if [ "$status" -ne 5 ]; then
        cat "$json_log_file" >&2
        fail "expected JSON compare mismatch exit code 5 for $asm_file"
    fi
    if ! grep -q '"match": false' "$json_log_file"; then
        cat "$json_log_file" >&2
        fail "expected JSON compare mismatch payload for $asm_file"
    fi
}

run_expect_xref_outputs() {
    asm_file="$TMPDIR/xref-fixture.asm"
    out_file="$TMPDIR/xref-fixture.bin"
    xref_json="$TMPDIR/xref.json"
    xref_json_locals="$TMPDIR/xref.locals.json"
    xref_csv_base="$TMPDIR/xrefcsv"
    log_file="$TMPDIR/xref.log"

    cat > "$asm_file" <<'ASM'
ORG $C000
Start:
@@loop:
    LDA #<Target
    JSR Target
    BNE @@loop
    RTS
Target:
    RTS
END
ASM

    if ! "$XASM" --pure-binary --xref="$xref_json" "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected xref JSON generation success"
    fi

    if [ ! -s "$xref_json" ]; then
        fail "xref JSON file not generated"
    fi

    if ! grep -q '"symbols"' "$xref_json"; then
        fail "xref JSON missing symbols array"
    fi

    if ! grep -q '"references"' "$xref_json"; then
        fail "xref JSON missing references array"
    fi

    if ! grep -q '"access":"call"' "$xref_json"; then
        fail "xref JSON missing call access classification"
    fi

    if grep -q '@@loop' "$xref_json"; then
        fail "xref JSON should exclude locals by default"
    fi

    if ! "$XASM" --pure-binary --xref="$xref_json_locals" --xref-include-locals=true "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected xref JSON generation with locals"
    fi

    if ! grep -q '@@loop' "$xref_json_locals"; then
        fail "xref JSON with locals should include local labels"
    fi

    if ! "$XASM" --pure-binary --xref="$xref_csv_base" --xref-format=csv --xref-include-locals=true "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected xref CSV generation success"
    fi

    if [ ! -s "$xref_csv_base.symbols.csv" ] || [ ! -s "$xref_csv_base.refs.csv" ]; then
        fail "xref CSV output files not generated"
    fi

    if ! head -n 1 "$xref_csv_base.symbols.csv" | grep -q '^name,kind,scope,defined,'; then
        fail "xref symbols CSV header missing"
    fi

    if ! head -n 1 "$xref_csv_base.refs.csv" | grep -q '^symbol,file,line,column,'; then
        fail "xref refs CSV header missing"
    fi
}

run_expect_phase1_spec_parity() {
    asm_file="$TMPDIR/spec-phase1.asm"
    out_file="$TMPDIR/spec-phase1.bin"
    xref_json="$TMPDIR/spec-phase1.xref.json"
    xref_locals_json="$TMPDIR/spec-phase1.xref.locals.json"
    listing_json="$TMPDIR/spec-phase1.listing.json"
    compare_ref="$TMPDIR/spec-phase1.ref.bin"
    compare_match_json="$TMPDIR/spec-phase1.compare.match.json"
    compare_mismatch_json="$TMPDIR/spec-phase1.compare.mismatch.json"
    log_file="$TMPDIR/spec-phase1.log"

    cat > "$asm_file" <<'ASM'
ORG $C000
Beta:
Alpha:
@@local:
  LDA #<Target
  LDA #>Target
  JSR Target
  JMP Target
  BNE @@local
  STA Buf
  LDA Buf
  DW Beta+Alpha
  DB 1,2,3,4,5,6
  RTS
Buf:
  DB 0
ZP EQU $80
Target:
  RTS
END
ASM

    if ! "$XASM" --pure-binary --listing="$listing_json" --listing-format=json --xref="$xref_json" "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "phase1 parity fixture failed to assemble"
    fi

    if [ ! -s "$xref_json" ]; then
        fail "phase1 parity xref JSON missing"
    fi
    if [ ! -s "$listing_json" ]; then
        fail "phase1 parity listing JSON missing"
    fi

    # xref JSON structure and deterministic symbol/reference ordering
    for k in '"version": "1"' '"build": {' '"symbols": [' '"references": [' '"timestamp_utc": '; do
        if ! grep -Fq "$k" "$xref_json"; then
            fail "phase1 parity xref JSON missing key: $k"
        fi
    done

    symbols_order=$(awk -F'"name":"' '/"name":"/ {split($2,a,"\""); print a[1]}' "$xref_json" | paste -sd' ' -)
    if [ "$symbols_order" != "Alpha Beta Buf Target ZP" ]; then
        fail "phase1 parity xref symbol ordering mismatch ($symbols_order)"
    fi

    refs_order=$(awk -F'"symbol":"' '/"symbol":"/ {split($2,a,"\""); print a[1]}' "$xref_json" | paste -sd' ' -)
    if [ "$refs_order" != "Target Target Target Target Buf Buf Alpha Beta" ]; then
        fail "phase1 parity xref reference ordering mismatch ($refs_order)"
    fi

    # Default xref must exclude local labels.
    if grep -q '@@local' "$xref_json"; then
        fail "phase1 parity xref unexpectedly includes local label by default"
    fi

    # Access classifications present in default output.
    for access in pointer_lo pointer_hi call jump write read address_compute; do
        if ! grep -q "\"access\":\"$access\"" "$xref_json"; then
            fail "phase1 parity xref missing access classification: $access"
        fi
    done

    # listing JSON structural checks + continuation semantics.
    for k in '"version": "1"' '"source_file":' '"output_file":' '"records": [' '"output_offset_start":' '"output_offset_end":' '"continuation_of_record":true'; do
        if ! grep -Fq "$k" "$listing_json"; then
            fail "phase1 parity listing JSON missing key/value: $k"
        fi
    done
    if ! grep -q '"addressing_mode":"immediate"' "$listing_json"; then
        fail "phase1 parity listing JSON missing addressing mode"
    fi

    # xref include-locals toggles local labels/references.
    if ! "$XASM" --pure-binary --xref="$xref_locals_json" --xref-include-locals=true "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "phase1 parity xref with locals failed"
    fi
    if ! grep -q '@@local#' "$xref_locals_json"; then
        fail "phase1 parity xref with locals missing local label"
    fi
    if ! grep -q '"access":"branch"' "$xref_locals_json"; then
        fail "phase1 parity xref with locals missing branch classification"
    fi

    # compare JSON match/mismatch behavior and exit codes.
    cp "$out_file" "$compare_ref"
    if ! "$XASM" --pure-binary --compare="$compare_ref" --compare-format=json "$asm_file" -o "$out_file" >"$compare_match_json" 2>"$log_file"; then
        cat "$log_file" >&2
        fail "phase1 parity compare match failed"
    fi
    for k in '"version": "1"' '"compared_length":' '"match": true' '"mismatches": ['; do
        if ! grep -Fq "$k" "$compare_match_json"; then
            fail "phase1 parity compare match JSON missing key/value: $k"
        fi
    done

    printf '\x00' | dd of="$compare_ref" bs=1 seek=0 conv=notrunc >/dev/null 2>&1
    set +e
    "$XASM" --pure-binary --compare="$compare_ref" --compare-max-mismatches=1 --compare-format=json "$asm_file" -o "$out_file" >"$compare_mismatch_json" 2>"$log_file"
    status=$?
    set -e
    if [ "$status" -ne 5 ]; then
        cat "$log_file" >&2
        fail "phase1 parity compare mismatch expected exit code 5 (got $status)"
    fi
    for k in '"match": false' '"expected_hex":' '"actual_hex":' '"source": {'; do
        if ! grep -Fq "$k" "$compare_mismatch_json"; then
            fail "phase1 parity compare mismatch JSON missing key/value: $k"
        fi
    done

    # xref CLI misuse should return spec exit code 2.
    set +e
    "$XASM" --xref="$TMPDIR/spec-phase1.bad" --xref-format=bogus "$asm_file" -o "$TMPDIR/spec-phase1.bad.bin" >"$log_file" 2>&1
    status=$?
    set -e
    if [ "$status" -ne 2 ]; then
        cat "$log_file" >&2
        fail "phase1 parity expected xref-format misuse exit code 2 (got $status)"
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
run_expect_success_pure_binary_with_listing_json "$ROOT_DIR/tests/coverage_org_pure.asm"
run_expect_success_pure_binary_with_listing_ndjson "$ROOT_DIR/tests/coverage_org_pure.asm"
run_expect_compare_match "$ROOT_DIR/tests/coverage_org_pure.asm"
run_expect_compare_mismatch "$ROOT_DIR/tests/coverage_org_pure.asm"
run_expect_xref_outputs
run_expect_phase1_spec_parity
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
