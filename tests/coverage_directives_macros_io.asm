MACRO add2 P1,P2
  LDA #P1
  CLC
  ADC #P2
ENDM
add2 1,2

INCLUDE "fixtures/include_target.asm"
INCSRC "fixtures/include_target.asm"
INCBIN "fixtures/data.bin"
CHARMAP "tests/fixtures/charmap.txt"

MESSAGE "directive coverage"
WARNING "warning coverage"
END
