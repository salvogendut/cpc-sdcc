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

compile_m4() {
    echo "Compiling (M4)..."
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o m4io.rel    "$SRC/m4io.c"
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o netinit.rel "$SRC/netinit_m4.c"
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o net.rel     "$SRC/net_m4.c"
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o dns.rel     "$SRC/dns_m4.c"
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o main.rel    main.c

    echo "Linking (M4)..."
    $CC -mz80 --nostdlib --no-std-crt0 \
        --code-loc 0x4000 \
        --data-loc 0x7000 \
        -o tcptest.ihx \
        crt0.rel m4io.rel netinit.rel net.rel dns.rel main.rel

    echo "Converting (M4)..."
    $MAKEBIN -p -o 0x4000 tcptest.ihx /tmp/tcptest_raw.bin
    python3 "$SRC/amsdos_wrap.py" /tmp/tcptest_raw.bin "$OUT_M4/TCPTEST.BIN" 4000
    rm -f /tmp/tcptest_raw.bin
    ls -l "$OUT_M4/TCPTEST.BIN"
}

compile ""              "ULIfAC/floppy" "$OUT"
compile "-DAMSDOS_USB"  "Albireo/USB"   "$OUT_ALB"
compile_m4

cp TCPTEST.BAS  "$OUT/"
cp TCPTESTA.BAS "$OUT_ALB/"
cp TCPTM4.BAS   "$OUT_M4/TCPTEST.BAS"

echo "Fixing CR+LF line endings in .BAS files..."
for f in "$OUT/TCPTEST.BAS" "$OUT_ALB/TCPTESTA.BAS" "$OUT_M4/TCPTEST.BAS"; do
    perl -pi -e 's/\r?\n/\r\n/' "$f"
done

echo ""
echo "ULIfAC:  run TCPTEST.BAS  from $OUT"
echo "Albireo: run TCPTESTA.BAS from $OUT_ALB"
echo "M4:      run TCPTEST.BAS  from $OUT_M4"
