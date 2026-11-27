#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The video types Wine's GStreamer source (the decodebin parser: what plays a file no splitter of
# Wine's own takes) offers downstream, in order (patches/sg/9260): after the stream's own format,
# the 4:2:0 ones first (NV12, YV12, I420) as Windows' decoders offer them, AYUV after them, RGB
# last. AYUV came first, so AviSynth's DirectShowSource (and any filter taking several types)
# got 4:4:4 frames (test/decoder-order-probe.c, which writes its own small AVI).
#   WINE=/opt/wine-sg/bin/wine test/decoder-order-gate.sh
# Mutant: SG_MUTANT_DECODER_AYUV_FIRST (winegstreamer quartz_parser.c)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-decoder-order.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/decoder-order-probe.c" -lole32 -luuid -lstrmiids || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 300 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check connect          "a file source connects to the GStreamer source"
check video_pin        "it has a video output pin"
check has_all          "the pin offers NV12, YV12, I420, AYUV and RGB32"
check nv12_first       "NV12 is the first conversion offered, as Windows' decoders"
check 420_before_ayuv  "the 4:2:0 formats come before AYUV"
check yuv_before_rgb   "YUV before RGB"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
