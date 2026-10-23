# D1 object wire layout

Status: **approved by the owner as part of D1 on 2026-10-23.**
Includes the owner-approved preimplementation erratum in semantic contract §9.
No runtime implementation is included here. This is the concrete
wire check following the corrected [semantic contract](../../XORCYST_V2_CONTRACTS.md).
The numbers below and the independent hex example were reviewed together.
Changing this encoding requires reopening the affected D1 contract.
It adds no language, v1 reader, cross-unit type system or linker analysis output.

## File organization

All integers are little-endian. `u32` is four bytes; expression integer literals
are their raw two's-complement 32 bits. No native C layout, padding or `long` is
serialized. Normal objects contain diagnostic locations; source text is optional.

`XOROBJ2\n` is both the discriminator and the format version. Any incompatible
wire change requires a different magic; there is no implicit product-version
negotiation or acceptance of unknown extensions.
The preimplementation withdrawal of unused tags 42–43 in that erratum retains
this magic and the existing example; no runtime v2 objects used those tags.

The 92-byte header is eight bytes `XOROBJ2\n`, a u32 exact file length, then ten
pairs `(table_offset, record_count)` in the order below. Offsets are absolute
file offsets. Zero records are legal. Records contain only u32 fields. Tables
are packed in this fixed order immediately after the header; each offset must
equal the preceding table's end, even for an empty table. Payload blobs follow
all tables. Check multiplication/addition in a wide counter before allocation
or narrowing. The representable file length is at most `UINT32_MAX`; normal
resource/allocation failures remain errors. No checksum replaces structural
validation, and no timestamp, host path to a tool or host pointer is stored.

| Table | Bytes/record | Fields, in order |
| --- | ---: | --- |
| Blobs | 8 | payload_offset, byte_length |
| Files | 12 | name_blob, text_present, text_blob |
| Locations | 20 | file, start_line, start_column, end_line, end_column |
| Definitions | 24 | name_blob, kind, flags, value, location_present, location |
| Expressions | 24 | operator, a, b, c, location_present, location |
| Extents | 36 | operator, a, b, c, d, name_present, name_blob, location_present, location |
| Anchors | 20 | kind, owner, position, location_present, location |
| Fixups | 24 | expression, element_width, map_present, map_blob, location_present, location |
| Fragments | 28 | kind, a, b, c, d, location_present, location |
| RAM objects | 20 | extent, alignment_exponent, zeropage, location_present, location |

Indices refer to the named table in the current input unit; zero is valid.
Every `*_present` is exactly 0 or 1. An absent optional index is encoded as 0
and is not dereferenced; a present index 0 references the first entry. Unused
operator fields are zero. No other flag bits, tags or trailing data are accepted.

Blobs appear in index order, packed after the tables, each followed by zero
padding to a four-byte boundary. Empty blobs occupy no payload bytes. Separate
users may share one blob index; payload regions cannot overlap. Byte values and
raw data may contain NUL; symbol/file names may not contain embedded NUL. Keep
existing source-name bytes rather than imposing a new UTF-8 language policy.
A character-map blob is exactly 256 bytes. A counted string is not a C string.

Locations use one-based lines/columns and an exclusive end, with end >= start.
These are diagnostic source spans, not the listing's inclusive end fields.
Unavailable spans use `location_present=0`, reported as "location unavailable";
normal frontend output must supply the required operand/count/definition spans.
An empty filename or invalid coordinate/index is not a substitute for absence.
Only optional rich source text may be dropped in ordinary emission. There is
no new strip command. Source IDs are unit-local, never merged by equal filenames.

## Definitions and expressions

Definition kind 1 holds an expression index; kind 2 an anchor index; kind 3 is
an import with value=0. Flag bit 0 marks an export; all other bits are zero.
An import cannot also be exported directly: an exported alias is a separate
expression definition referencing that import. Preserve captured definitions
as separate entries, even when their spellings match. Only exported names must
be unique in the final linked namespace. Keep definition order and source spans.
Labels, procedures and variables lower to anchors; provider value kinds come
from evaluation, not import annotations. Importer type hints have already served
local sizeof/member lowering and need no cross-unit type table.

Expression operands reference expression indices except where stated:

| Tag | Meaning | Fields |
| ---: | --- | --- |
| 1 | integer literal | a = raw 32 bits |
| 2 | string literal | a = blob |
| 3 | selected definition | a = definition |
| 4 | PC/address anchor | a = anchor |
| 5 | bank of symbol | a = definition; resolve provider ownership as in §5 of the semantic contract |
| 6 | selected type/extent value | a = extent; convert checked byte size to the existing 32-bit expression value, preserving frontend type capture; never used for deferred label sizeof |
| 16–20 | unary negate, complement, logical-not, low-byte, high-byte | a = operand |
| 32–39 | add, subtract, multiply, divide, remainder, shift-left, shift-right, bitwise-and | a,b = operands |
| 40–41 | bitwise-or, bitwise-xor | a,b = operands |
| 42–43 | reserved; invalid on input | no defined fields or operator semantics |
| 44–49 | equal, not-equal, less, less-equal, greater, greater-equal | a,b = operands |

`c` is zero for all current operators. Operator meanings, including string
concatenation/comparison, signed comparisons and wrapping arithmetic, are exactly
those in the semantic contract. Do not fold `x+0` until x's kind is resolved.
No literal host addresses or symbol-table slot numbers substitute for references.
The owner-approved D1 erratum in semantic contract section 9 withdraws logical
AND/OR: v1 has no source syntax or object operators for them. Tags 42–43 must
be rejected, not interpreted or renumbered; tags 44–49 remain unchanged.
`defined`, local/member lookup, masks, macro/conditional expansion and type
selection and explicit label-size queries are frontend operations; they cannot
escape as extra object tags. Any pending local operand-kind checks for label
sizeof must finish in the assembler before an object is published; there is no
wire guard or deferred query. The owner-selected exclusion of `.label name = expr`
is in the D1 migration ledger; this draft has no fixed-address label flag.

Definitions and expressions may forward-reference tables. Shared references
stay shared after decoding; no tree expansion is permitted. Cycles in required
value/extent dependencies report a located error. Loading bounds-valid tables
is not proof that their evaluations are valid. Unused equates keep the semantic
contract's existing unused-invalid behavior; do not eagerly evaluate them merely
because they are serialized.

## Checked extents and anchors

Extents serialize the already-required checked extent graph, distinct from
wrapping integer arithmetic. This avoids turning count*element-size into a
wrapped byte size before validation. Each node may carry its field name for
count diagnostics. `d` is zero for all operators.

| Tag | Operation | Fields and checks |
| ---: | --- | --- |
| 1 | literal byte extent | a = u32 bytes |
| 2 | count | a = expression; b,c = nonnegative minimum/maximum, b <= c <= INT32_MAX; require a known integer in that interval |
| 3 | sum | a,b = extents; checked addition |
| 4 | repeated extent | a = count extent, b = element extent; evaluate a first; zero returns zero without evaluating b; otherwise checked multiplication |
| 5 | field remainder | a = declared extent, b = initialized extent; both evaluated, require a >= b, checked subtraction |
| 6 | bounded extent | a = extent, b,c = minimum/maximum bytes; require b <= value <= c |
| 8 | string length | a = expression; counted byte length for a deferred data/initializer extent, not a deferred explicit label sizeof |

Intermediate extent counters are checked unsigned 64-bit; overflow is an error.
A conversion into a 32-bit expression is explicit (expression tag 6), not an
implicit wrapping of layout. Final per-storage/range limits remain the existing
ones in the semantic contract. Lowering must retain each original constraint,
including the declared count before subtracting initialized bytes. An omitted
empty outer array skips inner instances, but a negative count cannot be hidden
inside a sum. The decoder does not synthesize or delete these nodes. A field
remainder needs no separate wire fragment: it is storage with a remainder root
and the field's name/location. This is serialization of the existing contract,
not a new user-visible count rule.

Anchor kinds:

1. Fragment start: owner=fragment, position=0.
2. End of this unit's code sequence: owner=0, position=0.
3. RAM object base: owner=RAM object, position=0.
4. Data operand start: owner=fixup, position=0; it must belong to a data fragment.
5. Byte within raw data: owner=raw fragment, position=byte offset, at most its length.

Data operand starts include the resolved widths of preceding string operands.
Instruction fixups use their fragment-start anchor for `$`; branch displacement
uses start+2. Anchor differences inside a contiguous run use intervening extents
and can cancel an unresolved prefix. Nothing in the wire layout admits an
implicit origin constraint or merges independently allocated RAM objects.
Explicit `sizeof(label)` never becomes an extent node: the frontend either
captures its integer at the original source-time use or rejects it (§4.1 of the
semantic contract), equally for code and data labels. The proposed code-span tag
7 is withdrawn; that value is invalid. It is not a compatibility reservation,
and no object using it has been approved. Known `Msg: .char "HELLO" / .byte
sizeof(Msg)` lowers with literal 6; a forward/imported string in that scan is an
assembly error. Ordinary anchor subtraction such as `Tail-Head` remains a binary
expression over anchors, with the existing common-prefix extent analysis; it does
not need a serialized span tag. Anchor kind 2 still serves an ordinary label at
unit end. No object is allowed to smuggle an unresolved label-size query into
expression tag 6 or string-length tag 8.

## Fragments, fixups and RAM

Fragment table order is code source order within the unit, including zero-size
fragments. There is no object `.org`; linker script placement remains external.
Pure-binary origins never pass through the object encoder.

| Kind | Meaning | Fields |
| ---: | --- | --- |
| 1 | raw bytes | a = blob |
| 2 | data operands | a = first fixup, b = number of consecutive fixups |
| 3 | instruction | a = initial opcode; b = narrow_present; c = narrow opcode when present; d = fixup if the opcode has an operand, else 0 |
| 4 | storage | a = extent root |

Validate opcodes against the existing 6502 table. `narrow_present` is 0/1;
when present, c must be the legal zero-page counterpart of a. Explicit `.w`
lowerings have no candidate. When absent, c=0. Operandless opcodes have d=0
without dereferencing it; other opcodes require a valid fixup. Range checks use
the actual opcode and the direct/link policy, not a saved parser addressing mode.
Early literal truncation remains a frontend operation with its existing warning;
late fixups retain their values for final validation. First-walk narrowing and
permanent narrowing obey the semantic contract, irrespective of wire order of
expression records.

A data fixup's element_width is 1, 2 or 4. An instruction fixup uses 0, because
its opcode determines encoding. Only one-byte data fixups may have a map;
it applies to strings, exactly once, and never to numeric operands. Map-free
string data widens each byte to the element width. Every fixup has exactly one
owning fragment; no overlap between data ranges or sharing across instructions.
Expressions and map blobs, unlike fixups, may be shared freely. Empty data
ranges are legal; their first index must be at most the fixup count.

A RAM object's extent root describes the virtual source span from this data label
to the next emitted data label, or the unit's data-segment end, **before physical
placement**. Lower that interval's storage extents to a checked sum (or a literal
when known); include unlabeled storage and retain each count constraint.
It is not encoded as subtraction of two physical RAM anchors. It needs
no separate serialized RAM fragment stream: the selected interval is already the
RAM object's extent DAG. This allocation extent never supplies `sizeof(label)`:
that query has already become a source-time integer or assembly error. Adjacent
labels and interrupted data segments can make those two sizes different. A bare
label's zero-length interval is literal extent 0, not an invalid storage count.
It receives a suitable address without consuming bytes, so it can share an
address with the next allocated nonempty object.

Table order is v1's label order within that unit's expanded data stream (the order
`register_locals` observes), not alphabetical order or hash-table order. To
allocate, concatenate the RAM tables in unit link order. Source/input order only
builds this initial array; it is **not a stable tie key**. Use the exact v1
permutation below, then v1's first-fit allocator. Equal keys may reorder.

`alignment_exponent=0` means no explicit alignment requirement; 1..15 encodes an
explicit power-of-two alignment. `zeropage` is 0/1. For the sort key, v1 uses an
alignment value of **1 even when no alignment flag is set**; use 1 for the wire
value 0, otherwise use the explicit exponent. Do not mistakenly align all
unconstrained variables to two bytes. All key arithmetic/comparisons are u32:

```text
align_key = alignment_exponent if alignment_exponent != 0 else 1
key = (zeropage << 31) | (align_key << 24) | ((0x10000 - size) & 0xFFFFFFFF)

sort(A, p, r):
    if p >= r: return
    pivot = key(A[r])
    i = p - 1
    for j from p through r - 1:
        if key(A[j]) >= pivot:
            i += 1
            swap(A[i], A[j])
    swap(A[i + 1], A[r])
    q = i + 1
    sort(A, p, q - 1)
    sort(A, q + 1, r)

sort(A, 0, length(A) - 1)
```

This pins the permutation, not a requirement to use C recursion. Preserve the
same result if using an explicit stack or another optimized implementation.
For unconstrained sizes `1,2,1,2,1,2,4,1,2,1,1`, the zero-based original indices
must become `0,2,4,7,9,10,3,8,1,5,6`. The size-2 order is `3,8,1,5`, not stable
order `1,3,5,8`. Pin both one-unit and split-unit cases. All 15 required ROM
hashes remain gates: the stable-sort mutant changes dpadhero2, ntxdev and all
12 music ROMs, while bombsweeper is unchanged. It is a compatibility test,
not an accepted v2 byte change.

No code bytes are emitted for RAM objects. Definitions reference RAM anchors;
multiple definitions may reference one anchor. Preserve v1's existing bare-label
zero-size behavior and the approved count rules; add no broader zero-storage rule.

## Independent fixture and validation checklist

The source-tree evidence `object-wire-example.hex` was written from these field/offset tables, with no
XORcyst serializer. `check_wire_example.py` independently reads the literal
bytes and checks the offsets, references and decoded facts. It is evidence
tooling, not a proposed production loader. The example represents a public
`Start: .byte $EA` lowered to raw data, with location `input.asm:1:1–1:17`.
The file is 244 bytes. Header/table ends are 92,116,128,148,172,172,172,192,192,220,220;
blob payloads are at 220,232,240. At origin 0 it emits EA with Start=0;
at origin $8000 it emits the same byte with Start=$8000. No CPU address is
embedded in the object. Index-zero references are intentional.

The former 376-byte span fixture is withdrawn along with extent tag 7. It
encoded a link-time label-size query that v2 no longer supports. Frontend
capture/rejection is pinned by the source probes instead; a second wire form is
unnecessary for an ordinary integer literal.

Before production implementation, the review must check every mapping above
against the semantic examples. The eventual implementation tests must additionally
cover strings with mapped NUL, assembler-captured label-size literals,
RAM spans and exact quicksort permutation, shared expression/extent DAGs, imported aliases
and both link orders, zero field remainders, all narrowing controls and bank/RAM
ownership. Mutated inputs must reject unknown tags, nonzero reserved fields,
bad boolean flags, bad table/blob bounds, overlap, overflow, dangling references,
invalid fixup ownership, illegal opcode pairs, truncated data and v1 magic.
Resource/invalid-evaluation failures preserve old output. Tests must distinguish
malformed files from valid files whose required expressions later fail evaluation.
The example checks neither performance nor every production-loader invariant.

This layout and its fixture are included in the D1 approval recorded in
section 8 of the semantic contract. Implementation follows D0's staged gates;
installed tools stay v1 until cutover.
