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

# $1 = define for netinit  (controls how N4C.CFG is read)
# $2 = define for amsdos_in (controls CAS addresses for file I/O)
# ULIfAC: netinit has no define (poke mode — BASIC reads N4C.CFG and POKEs
#         values to &3F10); amsdos_in uses -DAMSDOS_USB so HTTPD.MAN and web
#         files are opened at the +3-shifted USB CAS addresses.
# Albireo: both use -DAMSDOS_USB (C reads N4C.CFG and files directly via CAS).
compile() {
    local net_flag=$1
    local io_flag=$2
    local label=$3
    local outdir=$4

    echo "Compiling ($label)..."
    $CC -mz80 --nostdlib --no-std-crt0 $net_flag -c -o w5100.rel      "$SRC/w5100.c"
    $CC -mz80 --nostdlib --no-std-crt0 $net_flag -c -o netinit.rel    "$SRC/netinit.c"
    $CC -mz80 --nostdlib --no-std-crt0 $net_flag -c -o net_multi.rel  "$SRC/net_multi.c"
    $CC -mz80 --nostdlib --no-std-crt0 $net_flag -c -o bank.rel       "$SRC/bank.c"
    $CC -mz80 --nostdlib --no-std-crt0 $io_flag  -c -o amsdos_in.rel  "$SRC/amsdos_in.c"
    $CC -mz80 --nostdlib --no-std-crt0 $net_flag -c -o cpcdetect.rel  "$SRC/cpcdetect.c"
    $CC -mz80 --nostdlib --no-std-crt0 $net_flag -c -o main.rel       main.c

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

compile ""             "-DAMSDOS_USB"  "ULIfAC"  "$OUT"
compile "-DAMSDOS_USB" "-DAMSDOS_USB"  "Albireo" "$OUT_ALB"

cp HTTPD.BAS  "$OUT/"
cp HTTDPA.BAS "$OUT_ALB/"
cp INDEX.HTM  "$OUT/"
cp INDEX.HTM  "$OUT_ALB/"
cp "$OUT/logo.png"    "$OUT/LOGO.PNG"
cp "$OUT/logo.png"    "$OUT_ALB/LOGO.PNG"
cp HTTPD.MAN.example "$OUT/HTTPD.MAN"
cp HTTPD.MAN.example "$OUT_ALB/HTTPD.MAN"

echo "Fixing CR+LF line endings in .BAS files..."
perl -pi -e 's/\r?\n/\r\n/' "$OUT/HTTPD.BAS"
perl -pi -e 's/\r?\n/\r\n/' "$OUT_ALB/HTTDPA.BAS"

echo "Fixing CR+LF line endings in web files..."
perl -pi -e 's/\r?\n/\r\n/' "$OUT/INDEX.HTM"
perl -pi -e 's/\r?\n/\r\n/' "$OUT_ALB/INDEX.HTM"
perl -pi -e 's/\r?\n/\r\n/' "$OUT/HTTPD.MAN"
perl -pi -e 's/\r?\n/\r\n/' "$OUT_ALB/HTTPD.MAN"

echo ""
echo "ULIfAC:  run HTTPD.BAS  from $OUT"
echo "Albireo: run HTTDPA.BAS from $OUT_ALB"
echo "Files needed on CPC disk: HTTPD.BIN N4C.CFG HTTPD.MAN INDEX.HTM LOGO.PNG"
