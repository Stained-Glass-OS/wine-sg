#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Colour contexts and thumbnails on the image encoders (patches/sg/2609):
# test/wicicc-probe.c. PNG, JPEG and TIFF write the profile and the decoders
# give it back; BMP and GIF refuse it; thumbnail, preview and container
# metadata requests answer by state.
#
#   WINE=/opt/wine-sg/bin/wine test/wicicc-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (windowscodecs): SG_MUTANT_ICC_PNG_NOT_WRITTEN (libpng.c, the iCCP
# chunk is left out), SG_MUTANT_ICC_JPEG_ONE_SEGMENT (libjpeg.c, only the first
# APP2 segment is written), SG_MUTANT_ICC_JPEG_FILE_ORDER (libjpeg.c, the segments
# are joined in file order, not by number), SG_MUTANT_ICC_JPEG_GAP_OK (libjpeg.c,
# a missing segment is skipped over), SG_MUTANT_ICC_TIFF_NOT_WRITTEN
# (libtiff.c), SG_MUTANT_ICC_NOT_REPLACED (encoder.c, a second list is appended
# to the first instead of replacing it), SG_MUTANT_ICC_STATE_UNCHECKED (encoder.c,
# SetColorContexts accepted after pixels were written).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wicicc.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/wicicc-probe.exe" "$HERE/wicicc-probe.c" \
    -lwindowscodecs -lole32 -loleaut32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# a crashed probe leaves wine helpers holding a pipe: go through a file
timeout -s KILL 60 "$WINE" "$T/wicicc-probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
