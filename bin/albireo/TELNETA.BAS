10 MEMORY &3DFF
20 PRINT "TELNET for Net4CPC"
30 PRINT "=================="
40 PRINT
50 INPUT "Host: ";hst$
60 IF hst$="" THEN PRINT "Cancelled.":END
70 prt=23
80 PRINT "Port (ENTER for 23): ";
90 INPUT pt$
100 IF pt$<>"" THEN prt=VAL(pt$)
110 IF prt=0 THEN prt=23
120 REM Write hostname to &3E00
130 addr=&3E00
140 FOR i=1 TO LEN(hst$):POKE addr,ASC(MID$(hst$,i,1)):addr=addr+1:NEXT i
150 POKE addr,0
160 POKE &3F0C,prt MOD 256
170 POKE &3F0D,INT(prt/256)
180 LOAD "TELNET.BIN",&4000
190 LOAD "CHARSET.BIN",&6800
200 CALL &4000
