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
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o net.rel     "$SRC/net.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o main.rel    main.c

    echo "Linking ($2)..."
    $CC -mz80 --nostdlib --no-std-crt0 \
        --code-loc 0x4000 \
        --data-loc 0x7000 \
        -o tcptest.ihx \
        crt0.rel w5100.rel netinit.rel net.rel main.rel

    echo "Converting ($2)..."
    $MAKEBIN -p -o 0x4000 tcptest.ihx /tmp/tcptest_raw.bin
    python3 "$SRC/amsdos_wrap.py" /tmp/tcptest_raw.bin "$3/TCPTEST.BIN" 4000
    rm -f /tmp/tcptest_raw.bin
    ls -l "$3/TCPTEST.BIN"
}

compile ""              "ULIfAC/floppy" "$OUT"
compile "-DAMSDOS_USB"  "Albireo/USB"   "$OUT_ALB"

cp TCPTEST.BAS  "$OUT/"
cp TCPTESTA.BAS "$OUT_ALB/"

echo "Fixing CR+LF line endings in .BAS files..."
for f in "$OUT/TCPTEST.BAS" "$OUT_ALB/TCPTESTA.BAS"; do
    perl -pi -e 's/\r?\n/\r\n/' "$f"
done

echo ""
echo "ULIfAC:  run TCPTEST.BAS  from $OUT"
echo "Albireo: run TCPTESTA.BAS from $OUT_ALB"
