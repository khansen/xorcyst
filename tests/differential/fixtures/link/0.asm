.extrn Text:byte
.extrn Count:byte
.extrn Address:word
.public Start
Start:
.db Text
.dsb Count
lda Address
.dw Address
