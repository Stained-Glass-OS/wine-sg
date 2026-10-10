#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# saving metadata: the writers' IWICPersistStream::Save / SaveEx / GetSizeMax for PNG chunks, GIF blocks,
# TIFF directories, Exif APP1 data and unknown data (patches/sg/2621): test/wicsave-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/wicsave-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (windowscodecs): SG_MUTANT_MDS_PNG_CRC_ZERO, SG_MUTANT_MDS_PNG_KEYWORD_ANY (pngformat.c),
# SG_MUTANT_MDS_IFD_UNSORTED, SG_MUTANT_MDS_IFD_NO_INLINE, SG_MUTANT_MDS_DIRTY_KEPT, SG_MUTANT_MDS_APP1_NO_SIG
# (metadatahandler.c), SG_MUTANT_MDS_GIF_COMMENT_UNSPLIT (gifformat.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wicsave.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/wicsave-probe.exe" "$HERE/wicsave-probe.c" \
    -lwindowscodecs -lole32 -loleaut32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# a crashed probe leaves wine helpers holding a pipe: go through a file
timeout -s KILL 60 "$WINE" "$T/wicsave-probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
