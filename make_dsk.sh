#!/bin/bash
set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$ROOT/bin"
BIN_M4="$ROOT/bin/m4"
SRC="$ROOT/src"
OUT="$ROOT/images"

# Find iDSK — set IDSK env var to override
IDSK="${IDSK:-$ROOT/../cpc-mastering/idsk}"
if [ ! -x "$IDSK" ]; then
    echo "iDSK not found at: $IDSK"
    echo "Build it from https://github.com/reidrac/cpc-mastering or set IDSK=/path/to/idsk"
    exit 1
fi

mkdir -p "$OUT"

# Prepare N4CCFG.BAS with CR+LF line endings in a temp dir (preserves filename for DSK)
TMP_DIR=$(mktemp -d)
TMP_N4CCFG="$TMP_DIR/N4CCFG.BAS"
perl -pe 's/\r?\n/\r\n/' "$SRC/N4CCFG.BAS" > "$TMP_N4CCFG"
trap "rm -rf $TMP_DIR" EXIT

make_disk() {
    local dsk=$1
    local dir=$2
    local n4ccfg=${3:-yes}   # "yes" = include N4CCFG.BAS (default), "no" = skip

    rm -f "$dsk"
    "$IDSK" "$dsk" -n

    if [ "$n4ccfg" = "yes" ]; then
        echo "  Adding N4CCFG.BAS..."
        "$IDSK" "$dsk" -i "$TMP_N4CCFG" -t 0 -f
    fi

    # ASCII files from disk dir (BAS, MAN, HTM)
    for f in "$dir"/*.BAS "$dir"/*.MAN "$dir"/*.HTM; do
        [ -f "$f" ] || continue
        echo "  Adding $(basename $f)..."
        "$IDSK" "$dsk" -i "$f" -t 0 -f
    done

    # Binary files — already have AMSDOS headers, iDSK detects them automatically
    for f in "$dir"/*.BIN "$dir"/*.PNG; do
        [ -f "$f" ] || continue
        echo "  Adding $(basename $f)..."
        "$IDSK" "$dsk" -i "$f" -f
    done

    echo "  Catalog:"
    "$IDSK" "$dsk" -l
}

echo "Creating ULIfAC disk image..."
make_disk "$OUT/n4c_tools.dsk" "$BIN"

echo ""
echo "Creating M4 disk image..."
if [ -d "$BIN_M4" ] && ls "$BIN_M4"/*.BIN 2>/dev/null | grep -q .; then
    make_disk "$OUT/m4_tools.dsk" "$BIN_M4" "no"
else
    echo "  No M4 binaries in $BIN_M4 — skipping M4 disk"
fi

echo ""
echo "Disk images written to $OUT/"
ls -lh "$OUT"/*.dsk
