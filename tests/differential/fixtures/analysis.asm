.dataseg
.org $10
Ptr .word
.codeseg
.org $8000
Entry:
lda #1
sta Ptr
ldx #0
Loop:
lda Table,x
inx
cpx #3
bne Loop
rts
Table: .db 1,2,3
.dw Entry
