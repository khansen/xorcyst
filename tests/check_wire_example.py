#!/usr/bin/env python3
"""Inspect the hand-written object-format example independently; not a runtime loader."""
from pathlib import Path
import struct


def inspect(name, length, counts):
    raw = bytes.fromhex((Path(__file__).resolve().parent / "fixtures" / name).read_text())
    assert raw[:8] == b"XOROBJ2\n"
    assert struct.unpack_from("<I", raw, 8)[0] == len(raw) == length
    widths = (8, 12, 20, 24, 24, 36, 20, 24, 28, 20)
    tables, end = [], 92
    for i, (width, expected_count) in enumerate(zip(widths, counts)):
        start, count = struct.unpack_from("<II", raw, 12 + i * 8)
        assert start == end and count == expected_count
        end = start + count * width
        assert end <= len(raw)
        tables.append([struct.unpack_from("<" + "I" * (width // 4), raw, start + j * width)
                       for j in range(count)])
    blobs = []
    for offset, size in tables[0]:
        assert offset == end
        next_end = (offset + size + 3) & ~3
        assert next_end <= len(raw) and not any(raw[offset + size:next_end])
        blobs.append(raw[offset:offset + size])
        end = next_end
    assert end == len(raw)
    return tables, blobs


t, b = inspect("object-wire-example.hex", 244, (3, 1, 1, 1, 0, 0, 1, 0, 1, 0))
assert b == [b"input.asm", b"Start", b"\xea"]
assert t[1] == [(0, 0, 0)]
assert t[2] == [(0, 1, 1, 1, 17)]
assert t[3] == [(1, 2, 1, 0, 1, 0)]
assert t[6] == [(1, 0, 0, 1, 0)]
assert t[8] == [(1, 2, 0, 0, 0, 1, 0)]
assert b[t[3][0][0]] == b"Start"
assert t[6][t[3][0][3]][1] == 0
assert b[t[8][0][1]] == b"\xea"
print("244-byte example: public Start; raw EA; location and index-zero references verified.")
