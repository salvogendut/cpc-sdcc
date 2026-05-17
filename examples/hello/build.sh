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
$MAKEBIN -p -o 0x4000 hello.ihx "$OUT/HELLO.BIN"

echo "Done: $OUT/HELLO.BIN"
echo ""
echo "On the CPC:"
echo '  LOAD "HELLO.BIN",0x4000'
echo "  CALL 0x4000"
