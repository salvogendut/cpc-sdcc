#!/usr/bin/env python3
"""Prepend a 128-byte AMSDOS type-2 (binary) header to a raw binary file."""
import sys, os

def make_amsdos_header(basename, data_len, load_addr):
    hdr = bytearray(128)
    name = basename.upper()[:8].ljust(8)
    hdr[0] = 0
    for i, c in enumerate(name):
        hdr[1 + i] = ord(c)
    hdr[9], hdr[10], hdr[11] = ord('B'), ord('I'), ord('N')
    hdr[18] = 2                          # file type: binary
    hdr[19] = data_len & 0xFF            # data length low
    hdr[20] = (data_len >> 8) & 0xFF     # data length high
    hdr[21] = load_addr & 0xFF           # load address low
    hdr[22] = (load_addr >> 8) & 0xFF    # load address high
    hdr[24] = data_len & 0xFF            # logical length low
    hdr[25] = (data_len >> 8) & 0xFF     # logical length high
    hdr[26] = load_addr & 0xFF           # entry address low
    hdr[27] = (load_addr >> 8) & 0xFF    # entry address high
    hdr[64] = data_len & 0xFF            # file length (RASM AMSDOS convention)
    hdr[65] = (data_len >> 8) & 0xFF
    return bytes(hdr)

if len(sys.argv) != 4:
    print(f"Usage: {sys.argv[0]} input.bin output.bin load_addr_hex", file=sys.stderr)
    sys.exit(1)

in_file, out_file, load_hex = sys.argv[1], sys.argv[2], sys.argv[3]
load_addr = int(load_hex, 16)
with open(in_file, 'rb') as f:
    data = f.read()
basename = os.path.splitext(os.path.basename(out_file))[0]
header = make_amsdos_header(basename, len(data), load_addr)
with open(out_file, 'wb') as f:
    f.write(header)
    f.write(data)
print(f"  AMSDOS: {len(data)} bytes at 0x{load_addr:04X} -> {out_file} ({len(data)+128} bytes total)")
