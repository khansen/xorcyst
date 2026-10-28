.charmap "map.txt"
.org $8000
Value = Later
Saved .equ Value
.dw Value
Value = Value+1
.dw Value, Saved
.undef Value
Value = 7
.char Text, Text
Later .equ 5
Text .equ "HI"
.public Value
