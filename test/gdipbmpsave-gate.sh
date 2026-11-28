#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What GDI+ writes for Image.Save(stream, ImageFormat.Bmp) (patches/sg/9463): test/gdipbmpsave-probe.c
# saves 1/4/8bpp indexed, 24bpp and 32bpp (P)ARGB bitmaps as BMP and reads the headers back.
#   - every BMP's bfSize is the whole stream: Wine's encoder left the colour table out of it, and
#     PDFsharp (which checks bfSize against the stream) refused every indexed image ("ReadIndexed-
#     MemoryBitmap: unsupported format"): NAPS2 6 could not save a page as PDF;
#   - a bitmap with alpha is a 32-bit BMP with a BITMAPINFOHEADER and BI_RGB, its alpha in each
#     pixel's fourth byte, as GDI+ writes it (Wine wrote WIC's BITMAPV4HEADER with bit fields, which
#     PDFsharp refuses for colour pages).
#
#   WINE=/opt/wine-sg/bin/wine test/gdipbmpsave-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: windowscodecs bmpencode.c SG_MUTANT_BMP_SIZE_NO_PALETTE, gdiplus image.c
#   SG_MUTANT_BMP_ALPHA_BITFIELDS.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-gdipbmpsave.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    sg_kill_prefix "$T/prefix"
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/gdipbmpsave-probe.exe" "$HERE/gdipbmpsave-probe.c" \
    -lgdiplus -lole32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

timeout -s KILL 60 "$WINE" "$T/gdipbmpsave-probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
sed 's/^/      /' "$T/out"
fails=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; fails=$((fails + 1)); }
f() { awk -v fmt="$1" -v key="$2" '$1 == "FMT" && $2 == fmt { for (i = 3; i < NF; i++) if ($i == key) print $(i + 1) }' "$T/out"; }
for fmt in 1bpp 4bpp 8bpp; do
    len=$(f $fmt len); bfs=$(f $fmt bfsize); off=$(f $fmt off); clr=$(f $fmt clr); hdr=$(f $fmt hdr)
    if [ -n "$len" ] && [ "$len" = "$bfs" ] && [ "$hdr" = 40 ] && [ "$off" = $((54 + clr * 4)) ] && [ "$clr" -gt 0 ]; then
        pass "$fmt indexed BMP: bfSize is the whole stream ($len), colour table counted, BITMAPINFOHEADER"
    else fail "$fmt indexed BMP: len $len bfSize $bfs off $off clr $clr hdr $hdr"; fi
done
len=$(f 24bpp len); bfs=$(f 24bpp bfsize)
[ -n "$len" ] && [ "$len" = "$bfs" ] && [ "$(f 24bpp hdr)" = 40 ] && [ "$(f 24bpp bits)" = 24 ] \
    && pass "24bpp BMP: BITMAPINFOHEADER, bfSize the stream" || fail "24bpp BMP: $(grep 'FMT 24bpp' "$T/out")"
for fmt in 32bppARGB 32bppPARGB; do
    len=$(f $fmt len)
    if [ -n "$len" ] && [ "$len" = "$(f $fmt bfsize)" ] && [ "$(f $fmt hdr)" = 40 ] && [ "$(f $fmt bits)" = 32 ] &&
       [ "$(f $fmt comp)" = 0 ] && [ "$(f $fmt off)" = 54 ] && [ "$(f $fmt alpha)" = 128 ]; then
        pass "$fmt BMP: 32 bits, BITMAPINFOHEADER, BI_RGB, the alpha kept in the fourth byte"
    else fail "$fmt BMP: $(grep "FMT $fmt " "$T/out")"; fi
done
[ $fails = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $((fails > 0))
