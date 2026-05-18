#!/bin/bash
set -e

SDCC_BIN=/var/home/salvogendut/Dev/sdcc/bin
AS=$SDCC_BIN/sdasz80
CC=$SDCC_BIN/sdcc
MAKEBIN=$SDCC_BIN/makebin
RASM=/var/home/salvogendut/.local/bin/rasm
N4CEWEN=/var/home/salvogendut/Dev/n4c-nettools/src/n4cewenterm

SRC=../../src
OUT=../../bin
OUT_ALB=../../bin/albireo

mkdir -p "$OUT" "$OUT_ALB"

echo "Building CHARSET.BIN from n4cewenterm charset.s..."
# RASM always writes CHARSET.BIN into the CWD; run it from /tmp
(cd /tmp && $RASM "$N4CEWEN/charset.s" 2>/dev/null) || true
# RASM output has a 128-byte AMSDOS header; strip it to get 2048 raw bytes
python3 -c "
data = open('/tmp/CHARSET.BIN','rb').read()
raw = data[128:] if len(data) == 2176 else data
open('/tmp/charset_raw.bin','wb').write(raw)
print(f'  Charset raw: {len(raw)} bytes')
"
python3 "$SRC/amsdos_wrap.py" /tmp/charset_raw.bin /tmp/CHARSET_WRAP.BIN 6800
cp /tmp/CHARSET_WRAP.BIN "$OUT/CHARSET.BIN"
cp /tmp/CHARSET_WRAP.BIN "$OUT_ALB/CHARSET.BIN"
rm -f /tmp/CHARSET.BIN /tmp/charset_raw.bin /tmp/CHARSET_WRAP.BIN
ls -l "$OUT/CHARSET.BIN"

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
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o screen.rel  screen.c
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o ansi.rel    ansi.c
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o main.rel    main.c

    echo "Linking ($label)..."
    $CC -mz80 --nostdlib --no-std-crt0 \
        --code-loc 0x4000 \
        --data-loc 0x7000 \
        -o telnet.ihx \
        crt0.rel w5100.rel netinit.rel udp.rel dns.rel net.rel \
        screen.rel ansi.rel main.rel

    echo "Converting ($label)..."
    $MAKEBIN -p -o 0x4000 telnet.ihx /tmp/telnet_raw.bin
    python3 "$SRC/amsdos_wrap.py" /tmp/telnet_raw.bin "$outdir/TELNET.BIN" 4000
    rm -f /tmp/telnet_raw.bin
    ls -l "$outdir/TELNET.BIN"
}

compile ""             "ULIfAC/floppy" "$OUT"
compile "-DAMSDOS_USB" "Albireo/USB"   "$OUT_ALB"

cp TELNET.BAS  "$OUT/"
cp TELNETA.BAS "$OUT_ALB/"

echo "Fixing CR+LF line endings in .BAS files..."
perl -pi -e 's/\r?\n/\r\n/' "$OUT/TELNET.BAS"
perl -pi -e 's/\r?\n/\r\n/' "$OUT_ALB/TELNETA.BAS"

echo ""
echo "ULIfAC:  run TELNET.BAS  from $OUT (needs CHARSET.BIN + TELNET.BIN + N4C.CFG)"
echo "Albireo: run TELNETA.BAS from $OUT_ALB (needs CHARSET.BIN + TELNET.BIN + N4C.CFG)"
