#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# t2embed: MicroType Express (MTX) compressed EOT fonts load (patches/sg/8231; TTLoadEmbeddedFont was
# E_ERRORDECOMPRESSINGFONTDATA). test/t2mtx-font.eot.b64 is test/t2mtx-font.py's font compressed with
# sfntly's open-source MTX encoder (Apache-2.0; its MtxWriter with a VDMX encoder added, since sfntly
# drops VDMX); the font t2embed decompresses is compared with the one this run builds: every table, and
# the glyphs by outline, components and instructions (test/t2mtx-fontcmp.py). Also an XOR-encrypted copy
# and a corrupt one (test/t2mtx-variants.py).
#   WINE=/opt/wine-sg/bin/wine test/t2mtx-gate.sh
# Mutant: SG_MUTANT_NO_MTX (t2embed/main.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${T2MTX_DPY:-286}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

[ -x /usr/bin/python3 ] && /usr/bin/python3 -c 'import fontTools' 2>/dev/null || { echo "SKIP: python3-fonttools missing"; exit 77; }
for need in Xvfb xdpyinfo base64 "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-t2mtx.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/t2mtx-probe.c" -lt2embed -lgdi32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
base64 -d "$HERE/t2mtx-font.eot.b64" >"$T/font.eot" &&
    /usr/bin/python3 -I "$HERE/t2mtx-variants.py" "$T/font.eot" "$T/xored.eot" "$T/corrupt.eot" &&
    /usr/bin/python3 -I "$HERE/t2mtx-font.py" "$T/original.ttf" || { echo "FAIL  the test files were not made"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
cd "$T" && timeout -s KILL 60 "$WINE" "$T/probe.exe" "Z:$T/font.eot" "Z:$T/xored.eot" "Z:$T/corrupt.eot" \
    "Z:$T/loaded.ttf" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1
if [ -s "$T/loaded.ttf" ]; then
    if /usr/bin/python3 -I "$HERE/t2mtx-fontcmp.py" "$T/original.ttf" "$T/loaded.ttf" >"$T/cmp" 2>&1; then
        echo "PASS  the decompressed font is the original: $(tail -1 "$T/cmp")"
    else
        sed 's/^/      /' "$T/cmp"; echo "FAIL  the decompressed font differs from the original"; rc=1
    fi
else
    echo "FAIL  no font data to compare"; rc=1
fi
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
