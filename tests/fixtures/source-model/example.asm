.ORG $8000
Cell .EQU $10
Entry: LDA Cell
@@again: BNE @@again
.DB "ab",<Entry
.DW Entry
.END
