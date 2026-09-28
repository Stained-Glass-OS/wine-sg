#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The interface fonts' names are families, as Windows keeps them
# (patches/sg/0462).
#
# Stained Glass OS sets the interface font to Segoe UI, substituted with
# Inter. SystemParametersInfo gave the enumerated face's full name instead --
# "Inter Regular", even for the bold caption font -- and programs look the
# name up as a family: Firefox found no family "Inter Regular" for CSS
# system-ui and drew its own pages (Welcome, New Tab widgets) in a serif font.
#
#   WINE=/opt/wine-sg/bin/wine test/metricsfont-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: needs python3"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
fc-list 2>/dev/null | grep -q "Inter" || { echo "SKIP: the Inter font (fonts-inter) is not installed"; exit 77; }

T=$(mktemp -d /var/tmp/sg-metricsfont.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/metricsfont-probe.exe" "$HERE/metricsfont-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
# the image's settings (sg-shell theme/52-sg-fonts.reg): Segoe UI 9 pt, the
# caption bold, and Segoe UI substituted with Inter
python3 - > "$T/fonts.reg" <<'PY'
import struct
lf = lambda w: struct.pack('<lllllBBBBBBBB', -12, 0, 0, 0, w, 0, 0, 0, 1, 0, 0, 0, 0) + 'Segoe UI'.encode('utf-16-le').ljust(64, b'\0')
h = lambda b: ','.join('%02x' % x for x in b)
print('Windows Registry Editor Version 5.00\n\n[HKEY_CURRENT_USER\\Control Panel\\Desktop\\WindowMetrics]')
print('"MenuFont"=hex:' + h(lf(400)))
print('"CaptionFont"=hex:' + h(lf(700)))
print('"IconFont"=hex:' + h(lf(400)))
print('\n[HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes]\n"Segoe UI"="Inter"')
PY
"$WINE" regedit /s "$T/fonts.reg" >/dev/null 2>&1
"$WINESERVER" -w
out=$("$WINE" "$T/metricsfont-probe.exe" 2>/dev/null | tr -d '\r')
echo "$out" | sed 's/^/      /'
echo "$out" | grep -qx "menu Segoe UI" && pass "the menu font (CSS system-ui in Firefox) is the family Segoe UI" || fail "menu font: $(echo "$out" | grep menu)"
echo "$out" | grep -qx "caption Segoe UI" && pass "the bold caption font too, not a face's full name" || fail "caption font: $(echo "$out" | grep caption)"
echo "$out" | grep -qx "icon Segoe UI" && pass "and the icon title font" || fail "icon font: $(echo "$out" | grep icon)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
