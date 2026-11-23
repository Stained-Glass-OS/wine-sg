#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# win32u: a TrueType font whose OS/2 ulCodePageRange1 is empty (Noto Kufi Arabic on Debian: only the
# Arabic Unicode range bit and ulCodePageRange2's ASMO 708 bit) gets its character sets from its Unicode
# ranges (patches/sg/9500), so it is enumerated as ARABIC_CHARSET (not OEM_CHARSET, Wine's mark for a
# face with no code page), an Arabic request picks it, and ANSI text drawn with it goes through code page
# 1256. Before, it had no character set at all ("returning DEFAULT_CHARSET" and, from Qt programs creating
# it as enumerated, "Untranslated charset 255": 5-7 Store apps). Pixel check: the cp1256 bytes for alef +
# beh draw what U+0627 U+0628 draw.
#   WINE=/opt/wine-sg/bin/wine test/fontcsb-gate.sh
# Mutant: SG_MUTANT_NO_USB_CHARSETS (dlls/win32u/opentype.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${FONTCSB_DPY:-320}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

[ -x /usr/bin/python3 ] && /usr/bin/python3 -c 'import fontTools' 2>/dev/null || { echo "SKIP: python3-fonttools missing"; exit 77; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-fontcsb.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/fontcsb-probe.c" -lgdi32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
/usr/bin/python3 -I "$HERE/fontcsb-font.py" "$T/font.ttf" || { echo "FAIL  the test font was not made"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
cd "$T" && timeout -s KILL 60 "$WINE" "$T/probe.exe" "Z:$T/font.ttf" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
