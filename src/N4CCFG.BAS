10 REM Net4CPC N4C.CFG generator
20 PRINT "Net4CPC configuration"
30 PRINT "====================="
40 PRINT
50 INPUT "IP address  : ",ip$
60 INPUT "Subnet mask : ",mk$
70 INPUT "Gateway     : ",gw$
80 INPUT "DNS server  : ",dn$
90 PRINT
100 OPENOUT "N4C.CFG"
110 PRINT #9,"IP="+ip$
120 PRINT #9,"MASK="+mk$
130 PRINT #9,"GW="+gw$
140 PRINT #9,"DNS="+dn$
150 CLOSEOUT
160 PRINT "N4C.CFG saved."
170 END
