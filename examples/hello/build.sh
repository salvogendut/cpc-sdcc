#!/bin/bash
set -e

SDCC_BIN=/var/home/salvogendut/Dev/sdcc/bin
AS=$SDCC_BIN/sdasz80
CC=$SDCC_BIN/sdcc
MAKEBIN=$SDCC_BIN/makebin

SRC=../../src
OUT=../../bin

mkdir -p "$OUT"

echo "Assembling crt0..."
$AS -o crt0.rel "$SRC/crt0.s"

echo "Compiling main.c..."
$CC -mz80 --nostdlib --no-std-crt0 -c -o main.rel main.c

echo "Linking..."
$CC -mz80 --nostdlib --no-std-crt0 \
    --code-loc 0x4000 \
    --data-loc 0x7000 \
    -o hello.ihx \
    crt0.rel main.rel

echo "Converting to binary..."
$MAKEBIN -p -o 0x4000 hello.ihx /tmp/hello_raw.bin
python3 "$SRC/amsdos_wrap.py" /tmp/hello_raw.bin "$OUT/HELLO.BIN" 4000
rm -f /tmp/hello_raw.bin
ls -l "$OUT/HELLO.BIN"

cp HELLO.BAS "$OUT/"

echo "Fixing CR+LF line endings in .BAS file..."
perl -pi -e 's/\r?\n/\r\n/' "$OUT/HELLO.BAS"

echo ""
echo "On the CPC: RUN \"HELLO.BAS\""
