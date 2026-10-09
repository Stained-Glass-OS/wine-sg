#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# IWICBitmapFlipRotator orientations and the pixel format conversions that were
# unimplemented (patches/sg/2606): test/wicxform-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/wicxform-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (windowscodecs): SG_MUTANT_FLIP_ORDER (fliprotate.c, quarter turns flip the wrong axis; they do
# not exchange x and y), SG_MUTANT_CONV_GRAY_ROUND (converter.c, narrow gray
# rounds instead of keeping the top bits), SG_MUTANT_CONV_X256 (converter.c, 8 to
# 16 bit expansion by 256 instead of 257), SG_MUTANT_CONV_IDX_LSB (converter.c,
# narrow palette indices packed least significant bit first),
# SG_MUTANT_CONV_NO_UNPREMUL (converter.c, 32bppPRGBA read without un-premultiplying).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wicxform.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/wicxform-probe.exe" "$HERE/wicxform-probe.c" \
    -lwindowscodecs -lole32 -loleaut32 -lpropsys -luuid || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 60 "$WINE" "$T/wicxform-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && ! printf '%s\n' "$out" | grep -q '^FAIL' && exit 0
printf '%s\n' "$out" | grep -q '^RESULT:' || echo "RESULT: FAIL (the probe gave no result)"
exit 1
