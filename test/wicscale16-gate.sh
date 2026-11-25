#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windowscodecs: IWICBitmapScaler's filtered modes (linear, cubic, Fant, high quality cubic) for 16-bit,
# float and half-float channel formats (patches/sg/9202). 9353 filtered 8-bit channels only; 16-bit
# PNG/TIFF, JPEG XR and float images were still nearest neighbour ("drawn as nearest neighbour"). Per
# format the scaled source keeps its pixel format, a one-pixel checkerboard halved by Fant is mid grey,
# black next to white enlarged by linear has greys between, an HDR float above 1.0 is kept and straight
# alpha keeps the colour. The probe is test/wicscale16-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/wicscale16-gate.sh
# Mutant: SG_MUTANT_WIC_SCALER_8BIT_ONLY (dlls/windowscodecs/scaler.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wicscale16.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/wicscale16-probe.c" -lole32 -luuid -lwindowscodecs 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; echo "RESULT: FAIL"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cd "$T" && timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r' >"$T/out"
cat "$T/out"
rc=0
grep -qx 'done=1' "$T/out" && grep -qx 'failures=0' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
