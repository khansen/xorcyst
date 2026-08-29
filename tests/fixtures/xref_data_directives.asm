RAM_Base .EQU $7000
WordStride .EQU 2

.MACRO POINTER_ENTRY PointerArgument
    .DW PointerArgument
.ENDM

.ORG $8000
CodeTarget:
    RTS

PointerOwner:
    .DW CodeTarget
    .DW RAM_Base
    .DW CodeTarget+WordStride, $0000, DataTarget
    .DB <CodeTarget, >CodeTarget
    .DW RAM_Base+(DifferenceEnd-DifferenceStart)
    .DW RAM_Base+$1E*WordStride
    .DW (CodeTarget-1)
    .DW RAM_Base+(OtherSegmentLabel-DifferenceStart)
    .DD CodeTarget+DataTarget
    .DB FarTarget
    .DB $00
    .DW DataTarget

DifferenceStart:
    .DB $11
DifferenceEnd:
    .DB $22
DataTarget:
    .DB $33
FarTarget:
    RTS

MacroOwner:
    POINTER_ENTRY CodeTarget
    POINTER_ENTRY DataTarget

LocalOwner:
@@local:
    .DW @@local

AnonymousOwner:
    .DW +
+:
    .DB $44

UnknownTarget:
.ORG $9000
OtherSegmentLabel:
    .DW UnknownTarget

.ORG $A000
RepeatedOwnerA:
    .DW CodeTarget
.ORG $A000
RepeatedOwnerB:
    .DW DataTarget
.ORG $B000
    .DW CodeTarget
.END
