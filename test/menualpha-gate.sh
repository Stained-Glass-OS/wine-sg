#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A menu item's 32 bpp bitmap with an alpha channel is drawn blended (patches/sg/8702).
# SWT gives menus premultiplied 32 bpp DIB sections (TuxGuitar's Measure menu, Eclipse, DBeaver);
# win32u copied them with BitBlt, so every transparent pixel was a black box around the icon
# (qa4 2026-11-22). Windows blends them. A 32 bpp bitmap whose alpha is all zero is opaque, as before.
# The probe opens a popup menu of two bitmap-only items and reads the menu window back:
# no black (item 0's transparent field), its red square, and item 1's blue all there.
#   WINE=/opt/wine-sg/bin/wine test/menualpha-gate.sh     (SG_XDISPLAY=N: Xvfb :N, default 312)
# Mutant: SG_MUTANT_MENU_BITMAP_NO_ALPHA (dlls/win32u/menu.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
D=${SG_XDISPLAY:-312}
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -e "/tmp/.X11-unix/X$D" ] && { echo "SKIP: display :$D is taken"; exit 77; }
T=$(mktemp -d /var/tmp/sg-menualpha.XXXXXX)
XP=
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/menualpha-probe.c" -lgdi32 -luser32 || { echo "FAIL  probe did not build"; exit 1; }
Xvfb ":$D" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
sleep 2
export DISPLAY=":$D" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
OUT=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
echo "  $OUT"
set -- $OUT
RC=0
if [ "${1:-}" = black ] && [ "${2:-x}" -eq 0 ] && [ "${4:-0}" -ge 60 ]; then
    echo "PASS  menualpha: the transparent field of a 32 bpp alpha bitmap is not black ($2 black, $4 red)"
else echo "FAIL  menualpha: a menu's alpha bitmap: '$OUT' (want black 0, red 64)"; RC=1; fi
if [ "${6:-0}" -ge 250 ]; then echo "PASS  a 32 bpp bitmap without alpha stays opaque ($6 blue)"
else echo "FAIL  the bitmap without alpha: '$OUT' (want blue 256)"; RC=1; fi
exit $RC
