#!/bin/bash
set -e

SDCC_BIN=/var/home/salvogendut/Dev/sdcc/bin
AS=$SDCC_BIN/sdasz80
CC=$SDCC_BIN/sdcc
MAKEBIN=$SDCC_BIN/makebin

SRC=../../src
OUT=../../bin

mkdir -p "$OUT"

echo "Assembling crt0..."
$AS -o crt0.rel "$SRC/crt0.s"

echo "Compiling..."
$CC -mz80 --nostdlib --no-std-crt0 -DAMSDOS_USB -c -o w5100.rel   "$SRC/w5100.c"
$CC -mz80 --nostdlib --no-std-crt0 -DAMSDOS_USB -c -o netinit.rel "$SRC/netinit.c"
$CC -mz80 --nostdlib --no-std-crt0 -DAMSDOS_USB -c -o udp.rel     "$SRC/udp.c"
$CC -mz80 --nostdlib --no-std-crt0 -DAMSDOS_USB -c -o dns.rel     "$SRC/dns.c"
$CC -mz80 --nostdlib --no-std-crt0 -DAMSDOS_USB -c -o main.rel    main.c

echo "Linking..."
$CC -mz80 --nostdlib --no-std-crt0 \
    --code-loc 0x4000 \
    --data-loc 0x7000 \
    -o dnstest.ihx \
    crt0.rel w5100.rel netinit.rel udp.rel dns.rel main.rel

echo "Converting to binary..."
$MAKEBIN -p -o 0x4000 dnstest.ihx "$OUT/DNSTEST.BIN"

ls -l "$OUT/DNSTEST.BIN"
echo ""
echo "On the CPC:"
echo '  LOAD "DNSTEST.BIN",0x4000'
echo "  CALL 0x4000"
