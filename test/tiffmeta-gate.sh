#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windowscodecs + gdiplus: the TIFF encoder writes the frame's IFD metadata (patches/sg/9506): GDI+ property
# items saved as TIFF (ImageDescription, Artist, Orientation, a private LONG tag; were dropped, "the TIFF
# encoder writes no metadata blocks") are in the file's first directory and load back as property items, an
# Exif tag is left out with its directory (not written); a tag set through a WIC TIFF frame's metadata query
# writer (/ifd/{ushort=305}) reads back through the decoder's query reader. The file is read by
# test/tiffmeta-check.py too.
#   WINE=/opt/wine-sg/bin/wine test/tiffmeta-gate.sh
# Mutants: SG_MUTANT_TIFF_NO_METADATA (dlls/windowscodecs/libtiff.c), SG_MUTANT_GDIP_TIFF_NO_PROPERTIES
# (dlls/gdiplus/image.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${TIFFMETA_DPY:-327}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-tiffmeta.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/tiffmeta-probe.c" -lole32 -luuid -lwindowscodecs -lgdi32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
cd "$T" && timeout -s KILL 60 "$WINE" "$T/probe.exe" "Z:$T/gdiplus.tif" "Z:$T/wic.tif" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1
if [ -s "$T/gdiplus.tif" ]; then /usr/bin/python3 -I "$HERE/tiffmeta-check.py" "$T/gdiplus.tif" || rc=1
else echo "FAIL  no TIFF written"; rc=1; fi
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
