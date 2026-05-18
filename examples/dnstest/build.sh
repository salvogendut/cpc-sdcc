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
    $MAKEBIN -p -o 0x4000 dnstest.ihx /tmp/dnstest_raw.bin
    python3 "$SRC/amsdos_wrap.py" /tmp/dnstest_raw.bin "$3/DNSTEST.BIN" 4000
    rm -f /tmp/dnstest_raw.bin
    ls -l "$3/DNSTEST.BIN"
}

compile ""              "ULIfAC/floppy" "$OUT"
compile "-DAMSDOS_USB"  "Albireo/USB"   "$OUT_ALB"

cp DNSTEST.BAS  "$OUT/"
cp DNSTESTA.BAS "$OUT_ALB/"

echo "Fixing CR+LF line endings in .BAS files..."
for f in "$OUT/DNSTEST.BAS" "$OUT_ALB/DNSTESTA.BAS"; do
    perl -pi -e 's/\r?\n/\r\n/' "$f"
done

echo ""
echo "ULIfAC:  run DNSTEST.BAS  from $OUT"
echo "Albireo: run DNSTESTA.BAS from $OUT_ALB"
