#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# JPEG Exif metadata (patches/sg/2605): test/jpegexif-probe.c. The JPEG decoder
# reported no metadata blocks; the Exif APP1 segment is now an app1 block with
# the first IFD as a child (/app1/ifd/{ushort=274} ...).
#
#   WINE=/opt/wine-sg/bin/wine test/jpegexif-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_JPEG_APP1_ANY (windowscodecs/libjpeg.c, every APP1 counts as
# Exif), SG_MUTANT_APP1_IFD_AT_8 (metadatahandler.c, ignores the IFD offset in
# the TIFF header), SG_MUTANT_APP1_LITTLE_ONLY (metadatahandler.c, big-endian
# Exif read as little-endian).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-jpegexif.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/jpegexif-probe.exe" "$HERE/jpegexif-probe.c" \
    -lwindowscodecs -lole32 -loleaut32 -lpropsys -luuid || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 60 "$WINE" "$T/jpegexif-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && ! printf '%s\n' "$out" | grep -q '^FAIL' && exit 0
printf '%s\n' "$out" | grep -q '^RESULT:' || echo "RESULT: FAIL (the probe gave no result)"
exit 1
