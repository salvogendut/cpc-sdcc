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
    local label=$2
    local outdir=$3

    echo "Compiling ($label)..."
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o w5100.rel   "$SRC/w5100.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o netinit.rel "$SRC/netinit.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o udp.rel     "$SRC/udp.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o dns.rel     "$SRC/dns.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o net.rel     "$SRC/net.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o main.rel    main.c

    echo "Linking ($label)..."
    $CC -mz80 --nostdlib --no-std-crt0 \
        --code-loc 0x4000 \
        --data-loc 0x7000 \
        -o wget.ihx \
        crt0.rel w5100.rel netinit.rel udp.rel dns.rel net.rel main.rel

    echo "Converting ($label)..."
    $MAKEBIN -p -o 0x4000 wget.ihx /tmp/wget_raw.bin
    python3 "$SRC/amsdos_wrap.py" /tmp/wget_raw.bin "$outdir/WGET.BIN" 4000
    rm -f /tmp/wget_raw.bin
    ls -l "$outdir/WGET.BIN"
}

compile ""             "ULIfAC/floppy" "$OUT"
compile "-DAMSDOS_USB" "Albireo/USB"   "$OUT_ALB"

cp WGET.BAS   "$OUT/"
cp WGETA.BAS  "$OUT_ALB/"

echo "Fixing CR+LF line endings in .BAS files..."
perl -pi -e 's/\r?\n/\r\n/' "$OUT/WGET.BAS"
perl -pi -e 's/\r?\n/\r\n/' "$OUT_ALB/WGETA.BAS"

echo ""
echo "ULIfAC:  run WGET.BAS  from $OUT"
echo "Albireo: run WGETA.BAS from $OUT_ALB"
