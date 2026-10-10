#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# metadata writers reach the files: PNG text/gamma/chromaticity/time chunks, a JPEG Exif segment,
# GIF graphic control/comment extensions (patches/sg/2623): test/wicenc-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/wicenc-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (windowscodecs): SG_MUTANT_MDE_PNG_NOT_WRITTEN (libpng.c), SG_MUTANT_MDE_JPEG_NOT_WRITTEN (libjpeg.c),
# SG_MUTANT_MDE_GIF_NO_GCE, SG_MUTANT_MDE_FRAME_IGNORES_BLOCKS (gifformat.c / encoder.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wicenc.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/wicenc-probe.exe" "$HERE/wicenc-probe.c" \
    -lwindowscodecs -lole32 -loleaut32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# a crashed probe leaves wine helpers holding a pipe: go through a file
timeout -s KILL 60 "$WINE" "$T/wicenc-probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
