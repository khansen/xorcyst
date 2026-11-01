"""Test-only 6502 opcode and access oracle, independent of producer tables."""
# The 151 official 6502 opcodes, written out independently of xasm's tables.
EIGHT_MODES = ("immediate", "zeropage", "zeropage_x", "absolute", "absolute_x",
               "absolute_y", "preindexed_indirect", "postindexed_indirect")
OPCODES = {}
for mnemonic, codes in {
        "ADC": (0x69, 0x65, 0x75, 0x6D, 0x7D, 0x79, 0x61, 0x71),
        "AND": (0x29, 0x25, 0x35, 0x2D, 0x3D, 0x39, 0x21, 0x31),
        "CMP": (0xC9, 0xC5, 0xD5, 0xCD, 0xDD, 0xD9, 0xC1, 0xD1),
        "EOR": (0x49, 0x45, 0x55, 0x4D, 0x5D, 0x59, 0x41, 0x51),
        "LDA": (0xA9, 0xA5, 0xB5, 0xAD, 0xBD, 0xB9, 0xA1, 0xB1),
        "ORA": (0x09, 0x05, 0x15, 0x0D, 0x1D, 0x19, 0x01, 0x11),
        "SBC": (0xE9, 0xE5, 0xF5, 0xED, 0xFD, 0xF9, 0xE1, 0xF1)}.items():
    OPCODES.update({code: (mnemonic, mode) for code, mode in zip(codes, EIGHT_MODES)})
for mnemonic, base in {"ASL": 0x00, "ROL": 0x20, "LSR": 0x40, "ROR": 0x60}.items():
    OPCODES.update({base + 0x0A: (mnemonic, "accumulator"), base + 0x06: (mnemonic, "zeropage"),
                    base + 0x16: (mnemonic, "zeropage_x"), base + 0x0E: (mnemonic, "absolute"),
                    base + 0x1E: (mnemonic, "absolute_x")})
for mnemonic, codes in {
        "BIT": {0x24: "zeropage", 0x2C: "absolute"},
        "CPX": {0xE0: "immediate", 0xE4: "zeropage", 0xEC: "absolute"},
        "CPY": {0xC0: "immediate", 0xC4: "zeropage", 0xCC: "absolute"},
        "DEC": {0xC6: "zeropage", 0xD6: "zeropage_x", 0xCE: "absolute", 0xDE: "absolute_x"},
        "INC": {0xE6: "zeropage", 0xF6: "zeropage_x", 0xEE: "absolute", 0xFE: "absolute_x"},
        "JMP": {0x4C: "absolute", 0x6C: "indirect"},
        "JSR": {0x20: "absolute"},
        "LDX": {0xA2: "immediate", 0xA6: "zeropage", 0xB6: "zeropage_y", 0xAE: "absolute", 0xBE: "absolute_y"},
        "LDY": {0xA0: "immediate", 0xA4: "zeropage", 0xB4: "zeropage_x", 0xAC: "absolute", 0xBC: "absolute_x"},
        "STA": {0x85: "zeropage", 0x95: "zeropage_x", 0x8D: "absolute", 0x9D: "absolute_x",
                0x99: "absolute_y", 0x81: "preindexed_indirect", 0x91: "postindexed_indirect"},
        "STX": {0x86: "zeropage", 0x96: "zeropage_y", 0x8E: "absolute"},
        "STY": {0x84: "zeropage", 0x94: "zeropage_x", 0x8C: "absolute"}}.items():
    OPCODES.update({code: (mnemonic, mode) for code, mode in codes.items()})
OPCODES.update({code: (mnemonic, "relative") for mnemonic, code in {
    "BPL": 0x10, "BMI": 0x30, "BVC": 0x50, "BVS": 0x70,
    "BCC": 0x90, "BCS": 0xB0, "BNE": 0xD0, "BEQ": 0xF0}.items()})
OPCODES.update({code: (mnemonic, "implied") for mnemonic, code in {
    "BRK": 0x00, "PHP": 0x08, "CLC": 0x18, "PLP": 0x28, "SEC": 0x38, "RTI": 0x40,
    "PHA": 0x48, "CLI": 0x58, "RTS": 0x60, "PLA": 0x68, "SEI": 0x78, "DEY": 0x88,
    "TXA": 0x8A, "TYA": 0x98, "TXS": 0x9A, "TAY": 0xA8, "TAX": 0xAA, "CLV": 0xB8,
    "TSX": 0xBA, "INY": 0xC8, "DEX": 0xCA, "CLD": 0xD8, "INX": 0xE8, "NOP": 0xEA,
    "SED": 0xF8}.items()})
assert len(OPCODES) == 151

DATA_KINDS = dict.fromkeys(("LDA", "LDX", "LDY", "ADC", "SBC", "AND", "ORA", "EOR",
                            "CMP", "CPX", "CPY", "BIT"), "read")
DATA_KINDS.update(dict.fromkeys(("STA", "STX", "STY"), "write"))
DATA_KINDS.update(dict.fromkeys(("ASL", "LSR", "ROL", "ROR", "INC", "DEC"), "read_modify_write"))
MEMORY_MODES = ("zeropage", "zeropage_x", "zeropage_y", "absolute", "absolute_x", "absolute_y",
                "preindexed_indirect", "postindexed_indirect")
DATA_INDEX = {"zeropage_x": "X", "absolute_x": "X", "zeropage_y": "Y", "absolute_y": "Y",
              "postindexed_indirect": "Y"}


def expected_memory_access(record):
    mnemonic, mode = OPCODES[record["opcode"]]
    value = record["operand_value"]
    kind = DATA_KINDS.get(mnemonic) if mode in MEMORY_MODES else None
    pointer = mode in ("preindexed_indirect", "postindexed_indirect") or mode == "indirect"
    if kind is None and not pointer:
        return None
    return {
        "data": None if kind is None else {
            "kind": kind, "address": None if pointer else value,
            "index_register": DATA_INDEX.get(mode), "via_pointer": pointer},
        "pointer": None if not pointer else {
            "address": value,
            "high_byte_address": (value & 0xFF00) | ((value + 1) & 0xFF) if mode == "indirect"
                else (value + 1) & 0xFF,
            "index_register": "X" if mode == "preindexed_indirect" else None},
    }
