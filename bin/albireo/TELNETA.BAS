10 PRINT "TELNET for Net4CPC"
20 PRINT "=================="
30 PRINT
40 INPUT "Host: ";hst$
50 IF hst$="" THEN PRINT "Cancelled.":END
60 prt=23
70 PRINT "Port (ENTER for 23): ";
80 INPUT pt$
90 IF pt$<>"" THEN prt=VAL(pt$)
100 IF prt=0 THEN prt=23
110 REM Write hostname to &3E00
120 addr=&3E00
130 FOR i=1 TO LEN(hst$):POKE addr,ASC(MID$(hst$,i,1)):addr=addr+1:NEXT i
140 POKE addr,0
150 POKE &3F0C,prt MOD 256
160 POKE &3F0D,INT(prt/256)
165 MEMORY &3FFF
170 LOAD "TELNET.BIN",&4000
180 LOAD "CHARSET.BIN",&6800
190 CALL &4000
