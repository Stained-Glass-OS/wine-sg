#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# gdiplus: EMF+ EmfPlusSerializableObject records (patches/sg/9505; "Not implemented for EMF+ record type
# 56") -- an image effect's GUID and parameters ([MS-EMFPLUS] 2.3.5.2) -- give their effect to the images
# DrawImagePoints records with the E flag draw (2.3.4.9). A recorded metafile's red image, played with a
# colour matrix object (red and blue swapped) before its DrawImagePoints record with E, comes out blue;
# without E, or with E and no object in that playback, red. And an image
# drawn without image attributes (recorded with ImageAttributesID -1) plays back: it was InvalidParameter,
# so a metafile's plain images were never drawn.
#   WINE=/opt/wine-sg/bin/wine test/gdipserial-gate.sh
# Mutants: SG_MUTANT_GDIP_NO_SERIALIZABLE, SG_MUTANT_GDIP_ATTRS_REQUIRED (dlls/gdiplus/metafile.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${GDIPSERIAL_DPY:-325}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-gdipserial.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/gdipserial-probe.c" -lgdi32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
cd "$T" && timeout -s KILL 60 "$WINE" "$T/probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
