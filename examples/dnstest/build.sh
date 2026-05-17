#!/bin/bash
set -e

SDCC_BIN=/var/home/salvogendut/Dev/sdcc/bin
AS=$SDCC_BIN/sdasz80
CC=$SDCC_BIN/sdcc
MAKEBIN=$SDCC_BIN/makebin

SRC=../../src
OUT=../../bin
OUT_ALB=../../bin/albireo

mkdir -p "$OUT" "$OUT_ALB"

echo "Assembling crt0..."
$AS -o crt0.rel "$SRC/crt0.s"

compile() {
    local usb=$1
    echo "Compiling ($2)..."
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o w5100.rel   "$SRC/w5100.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o netinit.rel "$SRC/netinit.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o udp.rel     "$SRC/udp.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o dns.rel     "$SRC/dns.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o main.rel    main.c

    echo "Linking ($2)..."
    $CC -mz80 --nostdlib --no-std-crt0 \
        --code-loc 0x4000 \
        --data-loc 0x7000 \
        -o dnstest.ihx \
        crt0.rel w5100.rel netinit.rel udp.rel dns.rel main.rel

    echo "Converting ($2)..."
    $MAKEBIN -p -o 0x4000 dnstest.ihx "$3/DNSTEST.BIN"
    ls -l "$3/DNSTEST.BIN"
}

compile ""              "ULIfAC/floppy" "$OUT"
compile "-DAMSDOS_USB"  "Albireo/USB"   "$OUT_ALB"

echo ""
echo "ULIfAC:  run DNSTEST.BAS on the CPC"
echo "Albireo: LOAD \"DNSTEST.BIN\",0x4000 : CALL 0x4000"
