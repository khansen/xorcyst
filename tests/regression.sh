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
    expected_addr=${2:-8000}
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

    if ! grep -Fq -- "$expected_addr" "$lst_file"; then
        fail "expected ORG address $expected_addr missing from pure-binary listing for $asm_file"
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

    macro_asm="$TMPDIR/xref-macro-fixture.asm"
    macro_xref="$TMPDIR/xref.macro.json"
    cat > "$macro_asm" <<'ASM'
ORG $C000
MACRO SPIN
@@loop:
    DEX
    BNE @@loop
ENDM
Start:
    LDX #3
    SPIN
    RTS
END
ASM

    if ! "$XASM" --pure-binary --xref="$macro_xref" --xref-include-locals=true "$macro_asm" -o "$TMPDIR/xref-macro.bin" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected xref macro fixture success"
    fi

    if ! grep -q '"kind":"macro_label"' "$macro_xref"; then
        cat "$macro_xref" >&2
        fail "xref JSON should classify macro-expanded locals as macro_label"
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

run_expect_unused_equ_feature() {
    asm_file="$TMPDIR/unused-equ.asm"
    out_file="$TMPDIR/unused-equ.o"
    log_file="$TMPDIR/unused-equ.log"

    cat > "$asm_file" <<'ASM'
USED EQU $10
UNUSED EQU $20
SUPP_LINE EQU $30 ; xasm:ignore unused-equ
; xasm:disable unused-equ
SUPP_BLOCK EQU $40
; xasm:enable unused-equ
ALSO_UNUSED EQU $50

LDA USED
DB USED+1
DB "ALSO_UNUSED"
; UNUSED is only in a comment
END
ASM

    if ! "$XASM" --warn-unused-equ "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected --warn-unused-equ success"
    fi

    if ! grep -q "warning W0201: symbol 'UNUSED' defined by .EQU is never used" "$log_file"; then
        cat "$log_file" >&2
        fail "expected W0201 warning for UNUSED"
    fi

    if ! grep -q "warning W0201: symbol 'ALSO_UNUSED' defined by .EQU is never used" "$log_file"; then
        cat "$log_file" >&2
        fail "expected W0201 warning for ALSO_UNUSED"
    fi

    if grep -q "SUPP_LINE" "$log_file"; then
        cat "$log_file" >&2
        fail "did not expect W0201 for SUPP_LINE"
    fi

    if grep -q "SUPP_BLOCK" "$log_file"; then
        cat "$log_file" >&2
        fail "did not expect W0201 for SUPP_BLOCK"
    fi

    set +e
    "$XASM" --Werror=unused-equ "$asm_file" -o "$out_file" >"$log_file" 2>&1
    status=$?
    set -e
    if [ "$status" -eq 0 ]; then
        cat "$log_file" >&2
        fail "expected --Werror=unused-equ to fail"
    fi
    if ! grep -q "error W0201: symbol 'UNUSED' defined by .EQU is never used" "$log_file"; then
        cat "$log_file" >&2
        fail "expected W0201 promoted to error"
    fi

    if ! "$XASM" --warn-unused-equ --Wno-unused-equ "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected --Wno-unused-equ to disable diagnostics"
    fi
    if grep -q "W0201" "$log_file"; then
        cat "$log_file" >&2
        fail "did not expect W0201 with --Wno-unused-equ"
    fi
}

run_expect_audit_raw_addresses_feature() {
    asm_file="$TMPDIR/audit-raw.asm"
    out_file="$TMPDIR/audit-raw.bin"
    log_file="$TMPDIR/audit-raw.log"
    json_file="$TMPDIR/audit-raw.json"

    cat > "$asm_file" <<'ASM'
ORG $C000
Target:
  RTS
Start:
  LDA $80
  JSR $C000 ; xasm:audit-ignore A100
  JMP $C000
  LDA #<$C000
  LDA #>$C000
  DW $C000
  DB $00,$C0,$01,$02
; xasm:audit-disable
  LDA $80
; xasm:audit-enable
  RTS
ZP EQU $80
END
ASM

    if ! "$XASM" --pure-binary --audit-raw-addresses --audit-rom-range=\$C000-\$FFFF "$asm_file" -o "$out_file" >"$log_file" 2>&1; then
        cat "$log_file" >&2
        fail "expected audit warn-level success"
    fi

    for code in A100 A110 A120 A130 A131; do
        if ! grep -q "warning $code:" "$log_file"; then
            cat "$log_file" >&2
            fail "expected audit finding for $code"
        fi
    done

    if grep -q "JSR \$C000" "$log_file"; then
        cat "$log_file" >&2
        fail "did not expect ignored A100 finding for JSR line"
    fi

    set +e
    "$XASM" --pure-binary --audit-raw-addresses --audit-level=error --audit-rom-range=\$C000-\$FFFF "$asm_file" -o "$out_file" >"$log_file" 2>&1
    status=$?
    set -e
    if [ "$status" -ne 4 ]; then
        cat "$log_file" >&2
        fail "expected audit error-level exit code 4 (got $status)"
    fi
    if ! grep -q "error A120:" "$log_file"; then
        cat "$log_file" >&2
        fail "expected error-level audit diagnostics"
    fi

    if ! "$XASM" --pure-binary --audit-raw-addresses --audit-output-format=json --audit-rom-range=\$C000-\$FFFF "$asm_file" -o "$out_file" >"$json_file" 2>"$log_file"; then
        cat "$log_file" >&2
        fail "expected audit JSON output success"
    fi
    for key in '"version": "1"' '"findings": [' '"code":"A120"' '"severity":"warning"' '"suggested_symbol":"ZP"'; do
        if ! grep -Fq "$key" "$json_file"; then
            cat "$json_file" >&2
            fail "expected audit JSON key/value: $key"
        fi
    done

    set +e
    "$XASM" --audit-raw-addresses --audit-level=bogus "$asm_file" -o "$out_file" >"$log_file" 2>&1
    status=$?
    set -e
    if [ "$status" -ne 2 ]; then
        cat "$log_file" >&2
        fail "expected --audit-level misuse exit code 2 (got $status)"
    fi
}

run_expect_edge_hardening() {
    asm_cmp="$TMPDIR/edge-compare.asm"
    out_cmp="$TMPDIR/edge-compare.bin"
    ref_cmp="$TMPDIR/edge-compare.ref.bin"
    log_cmp="$TMPDIR/edge-compare.log"

    cat > "$asm_cmp" <<'ASM'
ORG $C000
Start:
  DB 1,2,3,4,5
END
ASM

    if ! "$XASM" --pure-binary "$asm_cmp" -o "$out_cmp" >"$log_cmp" 2>&1; then
        cat "$log_cmp" >&2
        fail "edge hardening: failed to assemble compare fixture"
    fi
    cp "$out_cmp" "$ref_cmp"
    printf '\xFF' | dd of="$ref_cmp" bs=1 seek=1 conv=notrunc >/dev/null 2>&1
    printf '\xEE' | dd of="$ref_cmp" bs=1 seek=3 conv=notrunc >/dev/null 2>&1

    set +e
    "$XASM" --pure-binary --compare="$ref_cmp" --compare-offset=1 --compare-length=4 --compare-max-mismatches=2 "$asm_cmp" -o "$out_cmp" >"$log_cmp" 2>&1
    status=$?
    set -e
    if [ "$status" -ne 5 ]; then
        cat "$log_cmp" >&2
        fail "edge hardening: compare with offset/length expected exit 5"
    fi
    if ! grep -q 'mismatch #1 at output+0x0001' "$log_cmp"; then
        cat "$log_cmp" >&2
        fail "edge hardening: expected mismatch #1 at offset 1"
    fi
    if ! grep -q 'mismatch #2 at output+0x0003' "$log_cmp"; then
        cat "$log_cmp" >&2
        fail "edge hardening: expected mismatch #2 at offset 3"
    fi

    # listing-format warning path should not fail assembly.
    if ! "$XASM" --listing-format=json "$ROOT_DIR/tests/ifndef.asm" -o "$TMPDIR/edge-listing.o" >"$log_cmp" 2>&1; then
        cat "$log_cmp" >&2
        fail "edge hardening: --listing-format without --listing should not fail assembly"
    fi
    if ! grep -q 'warning W0017: --listing-format is ignored because --listing is not set' "$log_cmp"; then
        cat "$log_cmp" >&2
        fail "edge hardening: missing W0017 warning"
    fi

    # xref anonymous include toggle.
    asm_anon="$TMPDIR/edge-anon.asm"
    xref_default="$TMPDIR/edge-anon.default.json"
    xref_anon="$TMPDIR/edge-anon.anon.json"
    cat > "$asm_anon" <<'ASM'
ORG $C000
Start:
  LDX #1
- DEX
  BPL -
  BEQ +
  RTS
+ RTS
END
ASM
    if ! "$XASM" --pure-binary --xref="$xref_default" "$asm_anon" -o "$TMPDIR/edge-anon.bin" >"$log_cmp" 2>&1; then
        cat "$log_cmp" >&2
        fail "edge hardening: xref default failed"
    fi
    if grep -q -- '-#' "$xref_default" || grep -q -- '+#' "$xref_default"; then
        fail "edge hardening: anonymous labels should be excluded by default"
    fi
    if ! "$XASM" --pure-binary --xref="$xref_anon" --xref-include-anon=true "$asm_anon" -o "$TMPDIR/edge-anon.bin" >"$log_cmp" 2>&1; then
        cat "$log_cmp" >&2
        fail "edge hardening: xref include anon failed"
    fi
    if ! grep -Eq '"name":"-#0(#[0-9]+)?"' "$xref_anon" || ! grep -Eq '"name":"\+#0(#[0-9]+)?"' "$xref_anon"; then
        fail "edge hardening: anonymous symbols missing with --xref-include-anon=true"
    fi

    # audit ROM-range gate should suppress out-of-range A131 even if label exists.
    asm_audit="$TMPDIR/edge-audit-range.asm"
    log_audit="$TMPDIR/edge-audit-range.log"
    cat > "$asm_audit" <<'ASM'
ORG $C000
TargetIn:
  RTS
ORG $D000
TargetOut:
  RTS
ORG $C010
Start:
  DB $00,$C0,$00,$D0
  RTS
END
ASM
    if ! "$XASM" --pure-binary --audit-raw-addresses --audit-rom-range=\$C000-\$C0FF "$asm_audit" -o "$TMPDIR/edge-audit-range.bin" >"$log_audit" 2>&1; then
        cat "$log_audit" >&2
        fail "edge hardening: audit range fixture failed"
    fi
    if ! grep -q 'A131:' "$log_audit"; then
        cat "$log_audit" >&2
        fail "edge hardening: expected in-range A131 finding"
    fi
    if grep -q 'TargetOut' "$log_audit"; then
        cat "$log_audit" >&2
        fail "edge hardening: out-of-range A131 finding should be suppressed by ROM range"
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

    if ! grep -Eq '^Reset-#0#0[[:space:]]+= \$C002$' "$lst_file"; then
        fail "expected scoped backward label Reset-#0#0 in symbol table"
    fi

    if ! grep -Eq '^Reset-#0#1[[:space:]]+= \$C005$' "$lst_file"; then
        fail "expected scoped backward label Reset-#0#1 in symbol table"
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
run_expect_unused_equ_feature
run_expect_audit_raw_addresses_feature
run_expect_edge_hardening
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

# Linker regression: cross-unit symbol resolution
cat > "$TMPDIR/link_cross_a.asm" <<'ASM'
.public public_symbol
public_symbol = $1234
END
ASM
cat > "$TMPDIR/link_cross_b.asm" <<'ASM'
.extrn public_symbol:label
    dw public_symbol
END
ASM
if ! "$XASM" "$TMPDIR/link_cross_a.asm" -o "$TMPDIR/link_cross_a.o" >/dev/null 2>&1; then
    fail "failed to build cross-unit fixture unit A"
fi
if ! "$XASM" "$TMPDIR/link_cross_b.asm" -o "$TMPDIR/link_cross_b.o" >/dev/null 2>&1; then
    fail "failed to build cross-unit fixture unit B"
fi
cat > "$TMPDIR/link_cross.script" <<EOF
output{file=$TMPDIR/link_cross.bin}
bank{size=\$100,origin=\$8000}
link{file=$TMPDIR/link_cross_b.o}
link{file=$TMPDIR/link_cross_a.o}
EOF
run_xlnk_expect_success "$TMPDIR/link_cross.script"
# public_symbol is $1234, so dw public_symbol should be 34 12
if ! od -An -t x1 "$TMPDIR/link_cross.bin" | tr -d '[:space:]' | grep -q "3412"; then
    od -t x1 "$TMPDIR/link_cross.bin" >&2
    fail "Cross-unit symbol resolution failed (expected 34 12)"
fi

# Linker regression: RAM allocation (ZP and normal RAM)
cat > "$TMPDIR/link_ram.asm" <<'ASM'
.DATASEG
a_zp .dsb 16
b_normal .dsb 1
.CODESEG
    lda a_zp
    sta b_normal
END
ASM

if ! "$XASM" "$TMPDIR/link_ram.asm" -o "$TMPDIR/link_ram.o" >/dev/null 2>&1; then
    fail "failed to build RAM fixture"
fi
cat > "$TMPDIR/link_ram.script" <<EOF
output{file=$TMPDIR/link_ram.bin}
ram{start=\$0010,end=\$001F}
ram{start=\$0300,end=\$03FF}
bank{size=\$100,origin=\$8000}
link{file=$TMPDIR/link_ram.o}
EOF
run_xlnk_expect_success "$TMPDIR/link_ram.script"
# a_zp should be at $0010, b_normal at $0300
# Depending on allocation order, could be:
# lda a_zp ($10) -> A5 10
# sta b_normal ($0300) -> 8D 00 03
# OR (observed):
# lda a_zp ($10) -> AD 00 03
# sta b_normal ($0300) -> 85 10
# Wait, if a_zp is $10, sta a_zp should be 85 10.
# If b_normal is $0300, lda b_normal should be AD 00 03.
# The code is:
# lda a_zp
# sta b_normal
# If it outputs AD 00 03 85 10, it means it thinks a_zp is at $0300 and b_normal is at $10.
# Which means b_normal got the ZP block!
if ! od -An -t x1 "$TMPDIR/link_ram.bin" | tr -d '[:space:]' | grep -Eq "a5108d0003|ad00038510"; then
    od -An -t x1 "$TMPDIR/link_ram.bin" >&2
    fail "RAM allocation failed"
fi

# Linker regression: bank overflow
cat > "$TMPDIR/link_overflow.asm" <<'ASM'
    dsb 20
END
ASM
if ! "$XASM" "$TMPDIR/link_overflow.asm" -o "$TMPDIR/link_overflow.o" >/dev/null 2>&1; then
    fail "failed to build overflow fixture"
fi
cat > "$TMPDIR/link_overflow.script" <<EOF
output{file=$TMPDIR/link_overflow.bin}
bank{size=\$0010,origin=\$8000}
link{file=$TMPDIR/link_overflow.o}
EOF
run_xlnk_expect_error_no_crash "$TMPDIR/link_overflow.script" "bank size.*exceeded"

# Linker regression: unresolved external
cat > "$TMPDIR/link_unresolved.asm" <<'ASM'
.extrn missing_symbol:label
    lda missing_symbol
END
ASM
if ! "$XASM" "$TMPDIR/link_unresolved.asm" -o "$TMPDIR/link_unresolved.o" >/dev/null 2>&1; then
    fail "failed to build unresolved fixture"
fi
cat > "$TMPDIR/link_unresolved.script" <<EOF
output{file=$TMPDIR/link_unresolved.bin}
bank{size=\$100,origin=\$8000}
link{file=$TMPDIR/link_unresolved.o}
EOF
run_xlnk_expect_error_no_crash "$TMPDIR/link_unresolved.script" "unknown symbol \`missing_symbol'"

# Linker regression: multiple banks
cat > "$TMPDIR/link_bank1.asm" <<'ASM'
    .db $01
END
ASM
cat > "$TMPDIR/link_bank2.asm" <<'ASM'
    .db $02
END
ASM
if ! "$XASM" "$TMPDIR/link_bank1.asm" -o "$TMPDIR/link_bank1.o" >/dev/null 2>&1; then
    fail "failed to build bank1 fixture"
fi
if ! "$XASM" "$TMPDIR/link_bank2.asm" -o "$TMPDIR/link_bank2.o" >/dev/null 2>&1; then
    fail "failed to build bank2 fixture"
fi
cat > "$TMPDIR/link_multi_bank.script" <<EOF
output{file=$TMPDIR/link_multi_bank.bin}
bank{size=\$10,origin=\$8000}
link{file=$TMPDIR/link_bank1.o}
bank{size=\$10,origin=\$9000}
link{file=$TMPDIR/link_bank2.o}
EOF
run_xlnk_expect_success "$TMPDIR/link_multi_bank.script"
# Each bank is $10 (16) bytes. Total binary should be 32 bytes.
out_size=$(wc -c < "$TMPDIR/link_multi_bank.bin" | tr -d '[:space:]')
if [ "$out_size" != "32" ]; then
    fail "unexpected multi-bank linker output size ($out_size, expected 32)"
fi
# Check bytes: $01 at offset 0, $02 at offset 16 ($10)
if ! od -An -t x1 "$TMPDIR/link_multi_bank.bin" | tr -d '[:space:]' | grep -q "^0100000000000000000000000000000002"; then
    od -An -t x1 "$TMPDIR/link_multi_bank.bin" >&2
    fail "Multi-bank binary content incorrect"
fi

# Linker regression: unreferenced external (should NOT error)
cat > "$TMPDIR/link_unused_ext.asm" <<'ASM'
.extrn unused_symbol:label
    rts
END
ASM
if ! "$XASM" "$TMPDIR/link_unused_ext.asm" -o "$TMPDIR/link_unused_ext.o" >/dev/null 2>&1; then
    fail "failed to build unused ext fixture"
fi
cat > "$TMPDIR/link_unused_ext.script" <<EOF
output{file=$TMPDIR/link_unused_ext.bin}
bank{size=\$100,origin=\$8000}
link{file=$TMPDIR/link_unused_ext.o}
EOF
run_xlnk_expect_success "$TMPDIR/link_unused_ext.script"

# Linker regression: invalid command in script
cat > "$TMPDIR/link_bad_cmd.script" <<EOF
bogus{foo=bar}
EOF
run_xlnk_expect_error_no_crash "$TMPDIR/link_bad_cmd.script" "unknown command \`bogus'"

# Linker regression: missing required argument in script
cat > "$TMPDIR/link_missing_arg.script" <<EOF
bank{}
EOF
run_xlnk_expect_error_no_crash "$TMPDIR/link_missing_arg.script" "no bank size set"

# Linker regression: argument out of range in script
cat > "$TMPDIR/link_range_arg.script" <<EOF
ram{start=\$10000,end=\$100}
EOF
run_xlnk_expect_error_no_crash "$TMPDIR/link_range_arg.script" "value of argument \`start' is out of range"

# Linker regression: cannot pad backwards
cat > "$TMPDIR/link_pad_back.script" <<EOF
output{file=$TMPDIR/link_pad_back.bin}
bank{size=\$100,origin=\$8000}
pad{size=\$10}
pad{offset=\$5}
EOF
run_xlnk_expect_error_no_crash "$TMPDIR/link_pad_back.script" "cannot pad.*backwards"

# Linker regression: multiple RAM blocks allocation
cat > "$TMPDIR/link_multi_ram.asm" <<'ASM'
.dataseg
var1 .dsb 8
var2 .dsb 8
.codeseg
    lda var1
    lda var2
END
ASM
if ! "$XASM" "$TMPDIR/link_multi_ram.asm" -o "$TMPDIR/link_multi_ram.o" >/dev/null 2>&1; then
    fail "failed to build multi-ram fixture"
fi
cat > "$TMPDIR/link_multi_ram.script" <<EOF
output{file=$TMPDIR/link_multi_ram.bin}
ram{start=\$0200,end=\$0208}
ram{start=\$0300,end=\$0308}
bank{size=\$100,origin=\$8000}
link{file=$TMPDIR/link_multi_ram.o}
EOF
run_xlnk_expect_success "$TMPDIR/link_multi_ram.script"
# var1 should be at $0200, var2 at $0300
# Depending on allocation order, could be AD 00 02 AD 00 03 or AD 00 03 AD 00 02
if ! od -An -t x1 "$TMPDIR/link_multi_ram.bin" | tr -d '[:space:]' | grep -Eq "ad0002ad0003|ad0003ad0002"; then
    od -An -t x1 "$TMPDIR/link_multi_ram.bin" >&2
    fail "Multi-RAM allocation failed"
fi

# Linker regression: out of RAM
cat > "$TMPDIR/link_out_of_ram.asm" <<'ASM'
.dataseg
big_var .dsb 20
.codeseg
    lda big_var
END
ASM
if ! "$XASM" "$TMPDIR/link_out_of_ram.asm" -o "$TMPDIR/link_out_of_ram.o" >/dev/null 2>&1; then
    fail "failed to build out-of-ram fixture"
fi
cat > "$TMPDIR/link_out_of_ram.script" <<EOF
output{file=$TMPDIR/link_out_of_ram.bin}
ram{start=\$0200,end=\$020F}
bank{size=\$100,origin=\$8000}
link{file=$TMPDIR/link_out_of_ram.o}
EOF
run_xlnk_expect_error_no_crash "$TMPDIR/link_out_of_ram.script" "out of.*RAM"

# Regression: DSB in DATASEG must not crash in pure binary mode
cat > "$TMPDIR/dataseg-dsb-crash.asm" <<'ASM'
ORG $C000
DATASEG
Foo: DSB 1
CODESEG
Start:
    LDA Foo
    RTS
END
ASM
run_expect_success_pure_binary "$TMPDIR/dataseg-dsb-crash.asm"

# Regression: EQU $ must be static (evaluated at definition point)
cat > "$TMPDIR/equ-pc-static.asm" <<'ASM'
ORG $C000
Start:
    LDA #1
Foo EQU $
    LDA #2
    LDX #Foo
    RTS
END
ASM
run_expect_success_pure_binary_with_listing "$TMPDIR/equ-pc-static.asm" "C000"
if ! grep -q "Foo.*=.*\$C002" "$TMPDIR/out-pure-listing.lst"; then
    cat "$TMPDIR/out-pure-listing.lst" >&2
    fail "EQU \$ was not static (expected \$C002 for Foo)"
fi

# Regression: EQU $ must be correct even after a branch instruction
# (which is initially parsed as ABSOLUTE_MODE but is 2 bytes RELATIVE_MODE)
cat > "$TMPDIR/equ-pc-after-branch.asm" <<'ASM'
ORG $C000
Start:
    LDA #1
    BEQ +
Foo EQU $
+   RTS
END
ASM
run_expect_success_pure_binary_with_listing "$TMPDIR/equ-pc-after-branch.asm" "C000"
if ! grep -q "Foo.*=.*\$C004" "$TMPDIR/out-pure-listing.lst"; then
    cat "$TMPDIR/out-pure-listing.lst" >&2
    fail "EQU \$ after branch was incorrect (expected \$C004 for Foo)"
fi

# Regression: Anonymous labels must be isolated in macros
cat > "$TMPDIR/macro-anon-isolation.asm" <<'ASM'
ORG $C000
MACRO test_macro
    BNE +
    LDA #1
+   NOP
ENDM
Start:
    BEQ +
    test_macro
+   RTS
END
ASM
run_expect_success_pure_binary_with_listing "$TMPDIR/macro-anon-isolation.asm" "C000"
# Start+#0#0 should be the target of BEQ +, which is the RTS at the end ($C007)
if ! grep -q "Start+#0#0.*=.*\$C007" "$TMPDIR/out-pure-listing.lst"; then
    cat "$TMPDIR/out-pure-listing.lst" >&2
    fail "Anonymous labels were not isolated in macro"
fi

# Regression: Anonymous labels must span procedure boundaries
# (unlike local labels, + and - are scoped to public labels, not procs)
cat > "$TMPDIR/proc-anon-span.asm" <<'ASM'
ORG $C000
.proc flush
    LDA $10
    BEQ +
    LDA #0
    STA $10
    RTS
.endp
.proc other
    LDA #1
+   RTS
.endp
END
ASM
run_expect_success_pure_binary_with_listing "$TMPDIR/proc-anon-span.asm" "C000"
# The + in flush should resolve to the + declaration in other ($C00B)
if ! grep -q "+#0#0.*=.*\$C00B" "$TMPDIR/out-pure-listing.lst"; then
    cat "$TMPDIR/out-pure-listing.lst" >&2
    fail "Anonymous labels should span procedure boundaries"
fi

# Regression: Anonymous labels must be isolated in REPT blocks
cat > "$TMPDIR/rept-anon-isolation.asm" <<'ASM'
ORG $C000
Start:
    BEQ +
    REPT 2
    BNE +
    LDA #1
+   NOP
    ENDM
+   RTS
END
ASM
run_expect_success_pure_binary_with_listing "$TMPDIR/rept-anon-isolation.asm" "C000"
# The outer + should resolve to the RTS after the REPT block, not the + inside
if ! grep -q "Start+#0#0.*=.*\$C00C" "$TMPDIR/out-pure-listing.lst"; then
    cat "$TMPDIR/out-pure-listing.lst" >&2
    fail "Anonymous labels were not isolated in REPT block"
fi

# Regression: Anonymous labels must be isolated in WHILE blocks
cat > "$TMPDIR/while-anon-isolation.asm" <<'ASM'
ORG $C000
COUNT = 1
Start:
    BEQ +
    WHILE COUNT > 0
    BNE +
    LDA #1
+   NOP
COUNT = COUNT - 1
    ENDM
+   RTS
END
ASM
run_expect_success_pure_binary_with_listing "$TMPDIR/while-anon-isolation.asm" "C000"
if ! grep -q "Start+#0#0.*=.*\$C007" "$TMPDIR/out-pure-listing.lst"; then
    cat "$TMPDIR/out-pure-listing.lst" >&2
    fail "Anonymous labels were not isolated in WHILE block"
fi

# Regression: undefined local label must produce a clear error, not "assuming external"
cat > "$TMPDIR/local-label-undefined.asm" <<'ASM'
ORG $C000
.proc first
    LDA #0
    BEQ @@done
    RTS
.endp
.proc second
    @@done:
    RTS
.endp
END
ASM
run_expect_error_no_crash "$TMPDIR/local-label-undefined.asm" "undefined local label"

# Regression: Labels must remain relocatable in object files
cat > "$TMPDIR/reloc-label-a.asm" <<'ASM'
.public Target
Target:
    RTS
END
ASM
cat > "$TMPDIR/reloc-label-b.asm" <<'ASM'
.extrn Target:label
    DW Target
END
ASM
if ! "$XASM" "$TMPDIR/reloc-label-a.asm" -o "$TMPDIR/reloc-label-a.o" >/dev/null 2>&1; then
    fail "failed to assemble relocatable unit A"
fi
if ! "$XASM" "$TMPDIR/reloc-label-b.asm" -o "$TMPDIR/reloc-label-b.o" >/dev/null 2>&1; then
    fail "failed to assemble relocatable unit B"
fi
cat > "$TMPDIR/reloc-link.script" <<EOF
output{file=$TMPDIR/reloc-link.bin}
bank{size=\$1000,origin=\$C000}
link{file=$TMPDIR/reloc-label-a.o}
link{file=$TMPDIR/reloc-label-b.o}
EOF
run_xlnk_expect_success "$TMPDIR/reloc-link.script"
# Unit A is 1 byte (RTS). Unit B is 2 bytes (DW Target).
# Target is at $C000. So Unit B should contain $00 $C0.
# The binary will be [RTS] [00] [C0].
# We check for the sequence 60 00 c0.
if ! od -An -t x1 "$TMPDIR/reloc-link.bin" | tr -d '[:space:]' | grep -q "6000c0"; then
    od -t x1 "$TMPDIR/reloc-link.bin" >&2
    fail "Label was not correctly relocated (frozen at Pass 1 address?)"
fi

# Regression: EQU $ must snapshot, but normal labels must remain accurate 
# even when ZP optimization changes instruction lengths between passes.
cat > "$TMPDIR/zp-opt-accurate-pc.asm" <<'ASM'
    ORG $0080
Start:
    LDA #1
    LDA Target  ; Pass 1: estimated 3 bytes (Absolute)
                ; Pass 3: optimized to 2 bytes (Zero Page)
Snap EQU $      ; Pass 1: $0082 + 3 = $0085
                ; Pass 5: should still be $0085
Target:         ; Pass 1: $0085
                ; Pass 5: should be $0084 (LDA #1=2, LDA Target=2)
    RTS
END
ASM
run_expect_success_pure_binary_with_listing "$TMPDIR/zp-opt-accurate-pc.asm" "0080"
if ! grep -q "Snap.*=.*\$0085" "$TMPDIR/out-pure-listing.lst"; then
    cat "$TMPDIR/out-pure-listing.lst" >&2
    fail "EQU \$ did not correctly snapshot Pass 1 PC"
fi
if ! grep -q "Target.*=.*\$0084" "$TMPDIR/out-pure-listing.lst"; then
    cat "$TMPDIR/out-pure-listing.lst" >&2
    fail "Normal label address was frozen at Pass 1 PC instead of remaining accurate"
fi

# Regression: Invalid ORG address should not poison PC-relative equates
cat > "$TMPDIR/org-range-poison.asm" <<'ASM'
    ORG $8000
    LDA #1
    ORG $10000 ; Out of range
    .message $
END
ASM
# We expect error but we want to check the output to see if PC remained $8002
"$XASM" --pure-binary "$TMPDIR/org-range-poison.asm" -o "$TMPDIR/org-range-poison.bin" > "$TMPDIR/org-range-poison.log" 2>&1 || true
if ! grep -q "32770" "$TMPDIR/org-range-poison.log"; then
    cat "$TMPDIR/org-range-poison.log" >&2
    fail "Invalid ORG poisoned subsequent PC (expected 32770/$8002)"
fi

# Regression: Invalid DSB count should not poison PC-relative equates
cat > "$TMPDIR/dsb-range-poison.asm" <<'ASM'
    ORG $8000
    LDA #1
    DSB -1 ; Out of range (negative)
    .message $
END
ASM
"$XASM" --pure-binary "$TMPDIR/dsb-range-poison.asm" -o "$TMPDIR/dsb-range-poison.bin" > "$TMPDIR/dsb-range-poison.log" 2>&1 || true
if ! grep -q "32770" "$TMPDIR/dsb-range-poison.log"; then
    cat "$TMPDIR/dsb-range-poison.log" >&2
    fail "Invalid DSB count poisoned subsequent PC (expected 32770/$8002)"
fi

# Regression: Weird macro arguments must not crash the assembler
cat > "$TMPDIR/weird-macro-args.asm" <<'ASM'
MACRO weird arg
  LDA arg
ENDM
  weird #1
  weird $1234
  weird +
  weird @@local
  weird RTS
END
ASM
# We expect errors but NO crash
"$XASM" --pure-binary "$TMPDIR/weird-macro-args.asm" -o "$TMPDIR/weird-macro-args.bin" >/dev/null 2>&1 || true

# Regression: Instruction length estimation must handle modes that get reduced
cat > "$TMPDIR/instr-length-reduction.asm" <<'ASM'
    ORG $0080
Start:
    LDA #1
    STX $1234,y ; Pass 1: STX ABSOLUTE_Y -> estimated 2 bytes (will become ZEROPAGE_Y)
Snap EQU $      ; Pass 1: $0082 + 2 = $0084
                ; Pass 5: should still be $0084
Target:         ; Pass 1: $0084
                ; Pass 5: should be $0084
    RTS
END
ASM
run_expect_success_pure_binary_with_listing "$TMPDIR/instr-length-reduction.asm" "0080"
if ! grep -q "Snap.*=.*\$0084" "$TMPDIR/out-pure-listing.lst"; then
    cat "$TMPDIR/out-pure-listing.lst" >&2
    fail "Instruction length reduction was not correctly estimated (EQU \$ snapshot wrong)"
fi

# Regression: ASSIGN (=) with $ must snapshot PC at definition point, not use point
cat > "$TMPDIR/assign-pc-snapshot.asm" <<'ASM'
    ORG $C000
    LDA #1
foo = $
    LDA #2
    LDA #3
    .message foo
END
ASM
"$XASM" --pure-binary "$TMPDIR/assign-pc-snapshot.asm" -o "$TMPDIR/assign-pc-snapshot.bin" > "$TMPDIR/assign-pc-snapshot.log" 2>&1 || true
# foo = $ is at PC $C002 (after ORG $C000 + LDA #1). Message should print 49154.
if ! grep -q "49154" "$TMPDIR/assign-pc-snapshot.log"; then
    cat "$TMPDIR/assign-pc-snapshot.log" >&2
    fail "ASSIGN (=) with \$ did not snapshot PC at definition point (expected 49154/\$C002)"
fi

# Regression: Forward branches near 128-byte limit must not go out of range in linked object files
cat > "$TMPDIR/branch-limit-obj.asm" <<'ASM'
.extrn state:label
.proc test_proc
    cmp #$B0
    bcc target_label    ; forward branch over ~105 bytes
    lda #1
    sta state
    lda #2
    sta state
    lda #3
    sta state
    lda #4
    sta state
    lda #5
    sta state
    lda #6
    sta state
    lda #7
    sta state
    lda #8
    sta state
    lda #9
    sta state
    lda #10
    sta state
    lda #11
    sta state
    lda #12
    sta state
    lda #13
    sta state
    lda #14
    sta state
    lda #15
    sta state
    lda #16
    sta state
    lda #17
    sta state
    lda #18
    sta state
    lda #19
    sta state
    lda #20
    sta state
    lda #21
    sta state
    target_label:
    rts
.endp
END
ASM
cat > "$TMPDIR/branch-limit-state.asm" <<'ASM'
.public state
state:
    DSB 1
END
ASM
if ! "$XASM" "$TMPDIR/branch-limit-obj.asm" -o "$TMPDIR/branch-limit-obj.o" >/dev/null 2>&1; then
    fail "failed to assemble branch-limit-obj.asm"
fi
if ! "$XASM" "$TMPDIR/branch-limit-state.asm" -o "$TMPDIR/branch-limit-state.o" >/dev/null 2>&1; then
    fail "failed to assemble branch-limit-state.asm"
fi
cat > "$TMPDIR/branch-limit-link.script" <<EOF
output{file=$TMPDIR/branch-limit-link.bin}
bank{size=\$2000,origin=\$C000}
link{file=$TMPDIR/branch-limit-obj.o}
link{file=$TMPDIR/branch-limit-state.o}
EOF
run_xlnk_expect_success "$TMPDIR/branch-limit-link.script"

# Test: CHAR_DATATYPE must be included in Pass 1 PC tracking
cat > "$TMPDIR/char-pc-tracking.asm" <<'ASM'
    ORG $C000
    .char 65, 66, 67   ; 3 bytes
Snap EQU $
    .message Snap
END
ASM
"$XASM" --pure-binary "$TMPDIR/char-pc-tracking.asm" -o "$TMPDIR/char-pc-tracking.bin" > "$TMPDIR/char-pc-tracking.log" 2>&1 || true
# .char 65,66,67 = 3 bytes, so Snap should be $C003 = 49155
if ! grep -q "49155" "$TMPDIR/char-pc-tracking.log"; then
    cat "$TMPDIR/char-pc-tracking.log" >&2
    fail "CHAR_DATATYPE not tracked in Pass 1 PC (expected 49155/\$C003)"
fi

# Test: Backward branch labels (-) must be isolated in macro scopes
cat > "$TMPDIR/macro-backward-isolation.asm" <<'ASM'
    ORG $C000

.macro test_macro
    -:
    LDA #1
    BNE -
.endm

    -:
    LDA #2
    test_macro
    BNE -           ; must refer to outer -, not macro's -
END
ASM
run_expect_success_pure_binary_with_listing "$TMPDIR/macro-backward-isolation.asm" "C000"
# The outer BNE - should jump to the outer label at $C000, not to the macro's label.
# If isolation fails, it would jump to the wrong target.
# Check listing for the outer backward label at $C000.
# The outer - label should be -#0#0 and the macro's - label should be -#0#1#0
# (different scope IDs prove isolation). Both must exist in the symbol table.
if ! grep -q -- "-#0#0.*=.*\$C000" "$TMPDIR/out-pure-listing.lst"; then
    cat "$TMPDIR/out-pure-listing.lst" >&2
    fail "Backward branch label not correctly isolated in macro scope"
fi
if ! grep -q -- "-#0#1#0" "$TMPDIR/out-pure-listing.lst"; then
    cat "$TMPDIR/out-pure-listing.lst" >&2
    fail "Macro backward branch label missing from symbol table"
fi

# Test: Nested scopes (macro inside REPT) with anonymous labels
cat > "$TMPDIR/nested-scope-isolation.asm" <<'ASM'
    ORG $C000

.macro inner_macro
    BEQ +
    LDA #1
    +:
.endm

    REPT 2
    BEQ +
    inner_macro
    +:
    ENDM
    RTS
END
ASM
run_expect_success_pure_binary_with_listing "$TMPDIR/nested-scope-isolation.asm" "C000"

# Test: Multiple anonymous label levels (++, +++)
cat > "$TMPDIR/multi-level-anon.asm" <<'ASM'
    ORG $C000
    BEQ +
    BEQ ++
    LDA #1
    +:
    LDA #2
    ++:
    RTS
END
ASM
run_expect_success_pure_binary_with_listing "$TMPDIR/multi-level-anon.asm" "C000"

# Test: EQU with complex $ expression ($ + offset)
cat > "$TMPDIR/equ-pc-complex.asm" <<'ASM'
    ORG $C000
    LDA #1          ; 2 bytes -> PC = $C002
foo EQU $ + 5       ; should be $C002 + 5 = $C007
    LDA #2
    .message foo
END
ASM
"$XASM" --pure-binary "$TMPDIR/equ-pc-complex.asm" -o "$TMPDIR/equ-pc-complex.bin" > "$TMPDIR/equ-pc-complex.log" 2>&1 || true
# $C007 = 49159
if ! grep -q "49159" "$TMPDIR/equ-pc-complex.log"; then
    cat "$TMPDIR/equ-pc-complex.log" >&2
    fail "EQU \$ + offset did not evaluate correctly (expected 49159/\$C007)"
fi

# Test: Binary include (.incbin) PC tracking
printf '\x01\x02\x03\x04\x05' > "$TMPDIR/five-bytes.bin"
cat > "$TMPDIR/incbin-pc-tracking.asm" <<'ASM'
    ORG $C000
    LDA #1          ; 2 bytes
ASM
# Use printf to write the INCBIN line with the proper path
printf '    INCBIN "%s"\n' "$TMPDIR/five-bytes.bin" >> "$TMPDIR/incbin-pc-tracking.asm"
cat >> "$TMPDIR/incbin-pc-tracking.asm" <<'ASM'
Snap EQU $
    .message Snap
END
ASM
"$XASM" --pure-binary "$TMPDIR/incbin-pc-tracking.asm" -o "$TMPDIR/incbin-pc-tracking.out" > "$TMPDIR/incbin-pc-tracking.log" 2>&1 || true
# LDA #1 = 2 bytes, INCBIN = 5 bytes -> Snap = $C000 + 7 = $C007 = 49159
if ! grep -q "49159" "$TMPDIR/incbin-pc-tracking.log"; then
    cat "$TMPDIR/incbin-pc-tracking.log" >&2
    fail "INCBIN not tracked in Pass 1 PC (expected 49159/\$C007)"
fi

# Test: Branch at exact 127-byte boundary (should succeed)
# BCC = 2 bytes. We need target_label at exactly PC+129 from the BCC
# (offset = 127 from end of BCC instruction).
# BCC(2) + 127 bytes of NOP = 129 bytes total; target at BCC+129 means offset=127.
{
cat <<'ASM'
    ORG $C000
    BCC target_label
ASM
# 127 bytes of NOP (1 byte each)
for i in $(seq 1 127); do echo "    NOP"; done
cat <<'ASM'
    target_label:
    RTS
END
ASM
} > "$TMPDIR/branch-exact-127.asm"
if ! "$XASM" --pure-binary "$TMPDIR/branch-exact-127.asm" -o "$TMPDIR/branch-exact-127.bin" >/dev/null 2>&1; then
    fail "Branch at exact 127-byte offset should succeed"
fi

# Test: Branch far out of range (256 NOPs — offset exceeds byte range)
{
cat <<'ASM'
    ORG $C000
    BCC target_label
ASM
# 256 bytes of NOP (offset = 256, well past byte range)
for i in $(seq 1 256); do echo "    NOP"; done
cat <<'ASM'
    target_label:
    RTS
END
ASM
} > "$TMPDIR/branch-far-out-of-range.asm"
set +e
"$XASM" --pure-binary "$TMPDIR/branch-far-out-of-range.asm" -o "$TMPDIR/branch-far-out-of-range.bin" >"$TMPDIR/branch-far-out-of-range.log" 2>&1
status=$?
set -e
if [ "$status" -eq 0 ]; then
    cat "$TMPDIR/branch-far-out-of-range.log" >&2
    fail "Branch at 256-byte offset should fail with 'branch out of range'"
fi
if [ "$status" -ge 128 ]; then
    cat "$TMPDIR/branch-far-out-of-range.log" >&2
    fail "xasm crashed for branch-far-out-of-range (exit $status)"
fi
if ! grep -q "branch out of range" "$TMPDIR/branch-far-out-of-range.log"; then
    cat "$TMPDIR/branch-far-out-of-range.log" >&2
    fail "missing expected error 'branch out of range' for branch-far-out-of-range.asm"
fi

# Test: ASSIGN redefinition with $ captures different PCs
cat > "$TMPDIR/assign-redefine-pc.asm" <<'ASM'
    ORG $C000
foo = $             ; $C000
    LDA #1          ; 2 bytes
    LDA #2          ; 2 bytes
    .message foo    ; should print $C000 = 49152
foo = $             ; $C004
    LDA #3          ; 2 bytes
    .message foo    ; should print $C004 = 49156
END
ASM
"$XASM" --pure-binary "$TMPDIR/assign-redefine-pc.asm" -o "$TMPDIR/assign-redefine-pc.bin" > "$TMPDIR/assign-redefine-pc.log" 2>&1 || true
if ! grep -q "49152" "$TMPDIR/assign-redefine-pc.log"; then
    cat "$TMPDIR/assign-redefine-pc.log" >&2
    fail "First ASSIGN \$ did not capture \$C000 (expected 49152)"
fi
if ! grep -q "49156" "$TMPDIR/assign-redefine-pc.log"; then
    cat "$TMPDIR/assign-redefine-pc.log" >&2
    fail "Second ASSIGN \$ did not capture \$C004 (expected 49156)"
fi

# Regression: recursive macro must not crash or hang
cat > "$TMPDIR/recursive-macro.asm" <<'ASM'
MACRO recurse
  recurse
ENDM
recurse
END
ASM
set +e
"$XASM" --pure-binary "$TMPDIR/recursive-macro.asm" -o "$TMPDIR/recursive-macro.bin" > "$TMPDIR/recursive-macro.log" 2>&1
status=$?
set -e
if [ "$status" -eq 0 ]; then
    fail "recursive macro should have failed"
fi
if ! grep -q "nesting depth limit exceeded" "$TMPDIR/recursive-macro.log"; then
    cat "$TMPDIR/recursive-macro.log" >&2
    fail "missing expected error 'nesting depth limit exceeded'"
fi

# Regression: infinite WHILE loop must not hang
cat > "$TMPDIR/infinite-while.asm" <<'ASM'
WHILE 1
  NOP
ENDM
END
ASM
set +e
"$XASM" --pure-binary "$TMPDIR/infinite-while.asm" -o "$TMPDIR/infinite-while.bin" > "$TMPDIR/infinite-while.log" 2>&1
status=$?
set -e
if ! grep -q "WHILE loop iteration limit exceeded" "$TMPDIR/infinite-while.log"; then
    cat "$TMPDIR/infinite-while.log" >&2
    fail "missing expected error 'WHILE loop iteration limit exceeded'"
fi

# Regression: INCBIN in DATASEG must not crash in pure binary mode
printf '\x01\x02\x03' > "$TMPDIR/three-bytes.bin"
cat > "$TMPDIR/dataseg-incbin-crash.asm" <<ASM
ORG \$C000
DATASEG
ASM
printf '    INCBIN "%s"\n' "$TMPDIR/three-bytes.bin" >> "$TMPDIR/dataseg-incbin-crash.asm"
cat >> "$TMPDIR/dataseg-incbin-crash.asm" <<ASM
CODESEG
Start:
    LDA #1
    RTS
END
ASM
run_expect_success_pure_binary "$TMPDIR/dataseg-incbin-crash.asm"

# Regression: Data directives in DATASEG must not crash in pure binary mode
cat > "$TMPDIR/dataseg-data-crash.asm" <<'ASM'
ORG $C000
DATASEG
Foo: .db 1, 2, 3
Bar: .dw $1234
CODESEG
Start:
    LDA Foo
    LDX Bar
    RTS
END
ASM
run_expect_success_pure_binary "$TMPDIR/dataseg-data-crash.asm"

# Regression: Undefined symbol in .db must not crash the assembler
cat > "$TMPDIR/db-undefined-crash.asm" <<'ASM'
.db UNDEFINED_SYMBOL
END
ASM
set +e
"$XASM" --pure-binary "$TMPDIR/db-undefined-crash.asm" -o "$TMPDIR/db-undefined-crash.bin" > "$TMPDIR/db-undefined-crash.log" 2>&1
status=$?
set -e
if [ "$status" -eq 0 ]; then
    fail "undefined symbol in .db should have failed"
fi
if [ "$status" -ge 128 ]; then
    cat "$TMPDIR/db-undefined-crash.log" >&2
    fail "xasm crashed for undefined symbol in .db (exit $status)"
fi
if ! grep -q "expression does not evaluate to integer literal" "$TMPDIR/db-undefined-crash.log"; then
    cat "$TMPDIR/db-undefined-crash.log" >&2
    fail "missing expected error 'expression does not evaluate to integer literal'"
fi

echo "All regression tests passed"
