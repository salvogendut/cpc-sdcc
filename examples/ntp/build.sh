#!/bin/bash
set -e

SDCC_BIN=/var/home/salvogendut/Dev/sdcc/bin
AS=$SDCC_BIN/sdasz80
CC=$SDCC_BIN/sdcc
MAKEBIN=$SDCC_BIN/makebin

SRC=../../src
OUT=../../bin
OUT_ALB=../../bin/albireo
OUT_M4=../../bin/m4

mkdir -p "$OUT" "$OUT_ALB" "$OUT_M4"

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
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o main.rel    main.c

    echo "Linking ($label)..."
    $CC -mz80 --nostdlib --no-std-crt0 \
        --code-loc 0x4000 \
        --data-loc 0x7000 \
        -o ntp.ihx \
        crt0.rel w5100.rel netinit.rel udp.rel dns.rel main.rel

    echo "Converting ($label)..."
    $MAKEBIN -p -o 0x4000 ntp.ihx /tmp/ntp_raw.bin
    python3 "$SRC/amsdos_wrap.py" /tmp/ntp_raw.bin "$outdir/NTP.BIN" 4000
    rm -f /tmp/ntp_raw.bin
    ls -l "$outdir/NTP.BIN"
}

compile_m4() {
    echo "Compiling (M4)..."
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o m4io.rel    "$SRC/m4io.c"
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o netinit.rel "$SRC/netinit_m4.c"
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o udp.rel     "$SRC/udp_m4.c"
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o dns.rel     "$SRC/dns_m4.c"
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o main.rel    main.c

    echo "Linking (M4)..."
    $CC -mz80 --nostdlib --no-std-crt0 \
        --code-loc 0x4000 \
        --data-loc 0x7000 \
        -o ntp.ihx \
        crt0.rel m4io.rel netinit.rel udp.rel dns.rel main.rel

    echo "Converting (M4)..."
    $MAKEBIN -p -o 0x4000 ntp.ihx /tmp/ntp_raw.bin
    python3 "$SRC/amsdos_wrap.py" /tmp/ntp_raw.bin "$OUT_M4/NTP.BIN" 4000
    rm -f /tmp/ntp_raw.bin
    ls -l "$OUT_M4/NTP.BIN"
}

compile ""             "ULIfAC/floppy" "$OUT"
compile "-DAMSDOS_USB" "Albireo/USB"   "$OUT_ALB"
compile_m4

cp NTP.BAS  "$OUT/"
cp NTPA.BAS "$OUT_ALB/"
cp NTPM4.BAS "$OUT_M4/NTP.BAS"

echo "Fixing CR+LF line endings in .BAS files..."
perl -pi -e 's/\r?\n/\r\n/' "$OUT/NTP.BAS"
perl -pi -e 's/\r?\n/\r\n/' "$OUT_ALB/NTPA.BAS"
perl -pi -e 's/\r?\n/\r\n/' "$OUT_M4/NTP.BAS"

echo ""
echo "ULIfAC:  run NTP.BAS  from $OUT"
echo "Albireo: run NTPA.BAS from $OUT_ALB"
echo "M4:      run NTP.BAS  from $OUT_M4"
