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
OUT_M4=../../bin/m4

mkdir -p "$OUT" "$OUT_ALB" "$OUT_M4"

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
cp /tmp/CHARSET_WRAP.BIN "$OUT_M4/CHARSET.BIN"
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
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o screen.rel   screen.c
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o ansi.rel     ansi.c
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o keyboard.rel keyboard.c
    $CC -mz80 --nostdlib --no-std-crt0 $usb -c -o main.rel     main.c

    echo "Linking ($label)..."
    $CC -mz80 --nostdlib --no-std-crt0 \
        --code-loc 0x4000 \
        --data-loc 0x7000 \
        -o telnet.ihx \
        crt0.rel w5100.rel netinit.rel udp.rel dns.rel net.rel \
        screen.rel ansi.rel keyboard.rel main.rel

    echo "Converting ($label)..."
    $MAKEBIN -p -o 0x4000 telnet.ihx /tmp/telnet_raw.bin
    python3 "$SRC/amsdos_wrap.py" /tmp/telnet_raw.bin "$outdir/TELNET.BIN" 4000
    rm -f /tmp/telnet_raw.bin
    ls -l "$outdir/TELNET.BIN"
}

compile_m4() {
    echo "Compiling (M4)..."
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o m4io.rel     "$SRC/m4io.c"
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o netinit.rel  "$SRC/netinit_m4.c"
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o net.rel      "$SRC/net_m4.c"
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o dns.rel      "$SRC/dns_m4.c"
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o screen.rel   screen.c
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o ansi.rel     ansi.c
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o keyboard.rel keyboard.c
    $CC -mz80 --nostdlib --no-std-crt0 -DNET_M4 -c -o main.rel     main.c

    echo "Linking (M4)..."
    $CC -mz80 --nostdlib --no-std-crt0 \
        --code-loc 0x4000 \
        --data-loc 0x7000 \
        -o telnet.ihx \
        crt0.rel m4io.rel netinit.rel net.rel dns.rel \
        screen.rel ansi.rel keyboard.rel main.rel

    echo "Converting (M4)..."
    $MAKEBIN -p -o 0x4000 telnet.ihx /tmp/telnet_raw.bin
    python3 "$SRC/amsdos_wrap.py" /tmp/telnet_raw.bin "$OUT_M4/TELNET.BIN" 4000
    rm -f /tmp/telnet_raw.bin
    ls -l "$OUT_M4/TELNET.BIN"
}

compile ""             "ULIfAC/floppy" "$OUT"
compile "-DAMSDOS_USB" "Albireo/USB"   "$OUT_ALB"
compile_m4

cp TELNET.BAS   "$OUT/"
cp TELNETA.BAS  "$OUT_ALB/"
cp TELNETM4.BAS "$OUT_M4/"

echo "Fixing CR+LF line endings in .BAS files..."
perl -pi -e 's/\r?\n/\r\n/' "$OUT/TELNET.BAS"
perl -pi -e 's/\r?\n/\r\n/' "$OUT_ALB/TELNETA.BAS"
perl -pi -e 's/\r?\n/\r\n/' "$OUT_M4/TELNETM4.BAS"

echo ""
echo "ULIfAC:  run TELNET.BAS   from $OUT     (needs CHARSET.BIN + TELNET.BIN + N4C.CFG)"
echo "Albireo: run TELNETA.BAS  from $OUT_ALB (needs CHARSET.BIN + TELNET.BIN + N4C.CFG)"
echo "M4:      run TELNETM4.BAS from $OUT_M4  (needs CHARSET.BIN + TELNET.BIN)"
