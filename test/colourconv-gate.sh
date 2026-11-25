#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Color Space Converter (patches/sg/9106; Wine had none, so OpenCV's DirectShow capture -- a
# Sample Grabber set to RGB24 behind an RGB32 source -- failed to connect): RGB32 to RGB24, RGB565
# and RGB555 through RenderStream, the frame's colours the right way up, a top-down source, no
# converter when the types match (test/colourconv-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/colourconv-gate.sh
# Mutants: SG_MUTANT_COLOUR_NO_FLIP (quartz colour.c)
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
T=$(mktemp -d /var/tmp/sg-colourconv.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/colourconv-probe.c" -lole32 -loleaut32 -luuid -lstrmiids || { fail "probe did not build"; exit 1; }
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
check create             "CLSID_Colour can be created"
check registered         "the filter mapper has the Color Space Converter"
check rgb24_connect      "RGB32 source to an RGB24 grabber connects"
check rgb24_converter    "through a Color Space Converter"
check rgb24_frame        "the RGB24 frame is right"
check rgb565_connect     "to an RGB565 grabber"
check rgb565_converter   "through the converter"
check rgb565_frame       "the RGB565 frame is right"
check rgb555_connect     "to an RGB555 grabber"
check rgb555_converter   "through the converter"
check rgb555_frame       "the RGB555 frame is right"
check rgb32_connect      "to an RGB32 grabber"
check rgb32_converter    "directly, no converter"
check rgb32_frame        "the RGB32 frame is right"
check topdown_connect    "a top-down RGB32 source to RGB24"
check topdown_converter  "through the converter"
check topdown_frame      "the frame is the right way up"
check flip_connect       "the converter takes a bottom-up type for a top-down source"
check flip_frame         "and turns the rows over"
check done               "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
