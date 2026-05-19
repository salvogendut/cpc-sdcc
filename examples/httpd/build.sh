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
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o w5100.rel      "$SRC/w5100.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o netinit.rel   "$SRC/netinit.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o net_multi.rel "$SRC/net_multi.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o bank.rel      "$SRC/bank.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o amsdos_in.rel "$SRC/amsdos_in.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o cpcdetect.rel "$SRC/cpcdetect.c"
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o main.rel      main.c

    echo "Linking ($label)..."
    $CC -mz80 --nostdlib --no-std-crt0 \
        --code-loc 0x4000 \
        --data-loc 0x7000 \
        -o httpd.ihx \
        crt0.rel w5100.rel netinit.rel net_multi.rel bank.rel amsdos_in.rel cpcdetect.rel main.rel

    echo "Converting ($label)..."
    $MAKEBIN -p -o 0x4000 httpd.ihx /tmp/httpd_raw.bin
    python3 "$SRC/amsdos_wrap.py" /tmp/httpd_raw.bin "$outdir/HTTPD.BIN" 4000
    rm -f /tmp/httpd_raw.bin
    ls -l "$outdir/HTTPD.BIN"
}

compile "-DAMSDOS_STD" "ULIfAC/floppy" "$OUT"
compile "-DAMSDOS_USB" "Albireo/USB"   "$OUT_ALB"

cp HTTPD.BAS  "$OUT/"
cp HTTDPA.BAS "$OUT_ALB/"

echo "Fixing CR+LF line endings in .BAS files..."
perl -pi -e 's/\r?\n/\r\n/' "$OUT/HTTPD.BAS"
perl -pi -e 's/\r?\n/\r\n/' "$OUT_ALB/HTTDPA.BAS"

echo ""
echo "ULIfAC:  run HTTPD.BAS  from $OUT (needs HTTPD.BIN + N4C.CFG + HTTPD.MAN + web files)"
echo "Albireo: run HTTDPA.BAS from $OUT_ALB (needs HTTPD.BIN + N4C.CFG + HTTPD.MAN + web files)"
