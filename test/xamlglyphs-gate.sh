#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml icon-font stand-ins (patches/sg/6158): text in Segoe MDL2 Assets or Segoe Fluent
# Icons (alone or first in a fallback list) on a system without them draws the icons as standard
# characters (Up, ChevronDown, Cancel, Refresh; Symbol values as before) -- the same pixels as those
# characters in DejaVu Sans. NanaZip's address bar Up button and chevron. test/xamlglyphs-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlglyphs-gate.sh
# Mutant (dlls/windows.ui.xaml/buttons.c): SG_MUTANT_XAML_NO_GLYPH_STANDINS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlglyphs.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlglyphs-probe.exe" "$HERE/xamlglyphs-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY
fc-list "DejaVu Sans" | grep -q . || { echo "SKIP: DejaVu Sans missing"; exit 77; }
fc-list | grep -qi "Segoe MDL2\|Segoe Fluent" && { echo "SKIP: the icon font is installed"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/xamlglyphs-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlglyphs-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup with icon-font text"
chk '^row0=1,1,0$' "E74A (Up) drawn as U+2191"
chk '^row1=1,1,0$' "E70D (ChevronDown) in a fallback list drawn as U+2228"
chk '^row2=1,1,0$' "E711 E72C (Cancel, Refresh) drawn as U+2715 U+21BB"
chk '^row3=1,1,0$' "E946 (Info, a Symbol value) still drawn as U+2139"
chk '^row4=1,1,0$' "E10B (Accept, a Symbol value) still drawn as U+2713"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
