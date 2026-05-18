10 REM WGET - HTTP File Downloader for Net4CPC
20 REM Memory layout written before CALL &4000:
30 REM   &3E00: Hostname (null-terminated)
40 REM   &3E80: Path (null-terminated)
50 REM   &3F00: Filename for CAS_OUT_OPEN (null-terminated)
60 REM   &3F0B: Filename length (1 byte)
70 REM   &3F0C: Port low byte, &3F0D: Port high byte
80 MODE 2:MEMORY &3DFF
90 PRINT "WGET for Net4CPC"
100 PRINT "================"
110 PRINT
120 INPUT "Enter URL: ";url$
130 IF url$="" THEN PRINT "Error: Empty URL":END
140 REM Strip scheme
150 IF LEFT$(url$,7)="http://" THEN url$=MID$(url$,8)
160 IF LEFT$(url$,8)="https://" THEN PRINT "Error: HTTPS not supported":END
170 REM Split host and path at first /
180 slash=INSTR(url$,"/")
190 IF slash=0 THEN hst$=url$:pth$="/":GOTO 240
200 hst$=LEFT$(url$,slash-1)
210 pth$=MID$(url$,slash)
220 REM
230 REM Extract port from host (e.g. host:8080)
240 prt=80
250 colon=INSTR(hst$,":")
260 IF colon>0 THEN prt=VAL(MID$(hst$,colon+1)):hst$=LEFT$(hst$,colon-1)
270 REM
280 REM Extract filename from path (last component after /)
290 REM
300 dfn$=""
310 FOR i=LEN(pth$) TO 1 STEP -1
320   IF MID$(pth$,i,1)="/" THEN GOTO 350
330   dfn$=MID$(pth$,i,1)+dfn$
340 NEXT i
350 IF dfn$="" THEN dfn$="INDEX.HTM"
360 REM
370 REM Convert filename to AMSDOS 8.3 uppercase format
380 REM
390 dot=INSTR(dfn$,".")
400 IF dot=0 THEN nm$=dfn$:ex$=""
410 IF dot>0 THEN nm$=LEFT$(dfn$,dot-1):ex$=MID$(dfn$,dot+1)
420 nm$=LEFT$(nm$,8):ex$=LEFT$(ex$,3)
430 temp$="":FOR i=1 TO LEN(nm$):c=ASC(MID$(nm$,i,1)):IF c>=97 AND c<=122 THEN c=c-32
440 temp$=temp$+CHR$(c):NEXT i:nm$=temp$
450 temp$="":FOR i=1 TO LEN(ex$):c=ASC(MID$(ex$,i,1)):IF c>=97 AND c<=122 THEN c=c-32
460 temp$=temp$+CHR$(c):NEXT i:ex$=temp$
470 WHILE LEN(nm$)<8:nm$=nm$+" ":WEND
480 WHILE LEN(ex$)<3:ex$=ex$+" ":WEND
490 REM Build dotted filename for CAS_OUT_OPEN
500 nm2$=nm$:WHILE LEN(nm2$)>0 AND RIGHT$(nm2$,1)=" ":nm2$=LEFT$(nm2$,LEN(nm2$)-1):WEND
510 ex2$=ex$:WHILE LEN(ex2$)>0 AND RIGHT$(ex2$,1)=" ":ex2$=LEFT$(ex2$,LEN(ex2$)-1):WEND
520 IF LEN(ex2$)>0 THEN cf$=nm2$+"."+ex2$ ELSE cf$=nm2$
530 REM
540 PRINT:PRINT "Host: ";hst$
550 PRINT "Path: ";pth$
560 PRINT "Port: ";prt
570 PRINT "Save as: ";LEFT$(nm$,8);".";LEFT$(ex$,3)
580 PRINT
590 INPUT "Continue? (Y/N) ";yn$
600 IF yn$<>"Y" AND yn$<>"y" THEN PRINT "Cancelled.":END
610 REM
620 REM Write parameters to fixed memory locations
630 REM
640 addr=&3E00
650 FOR i=1 TO LEN(hst$):POKE addr,ASC(MID$(hst$,i,1)):addr=addr+1:NEXT i
660 POKE addr,0
670 addr=&3E80
680 FOR i=1 TO LEN(pth$):POKE addr,ASC(MID$(pth$,i,1)):addr=addr+1:NEXT i
690 POKE addr,0
700 addr=&3F00
710 FOR i=1 TO LEN(cf$):POKE addr,ASC(MID$(cf$,i,1)):addr=addr+1:NEXT i
720 POKE &3F0B,LEN(cf$)
730 POKE &3F0C,prt MOD 256
740 POKE &3F0D,INT(prt/256)
750 REM
760 LOAD "WGET.BIN",&4000
770 CALL &4000
