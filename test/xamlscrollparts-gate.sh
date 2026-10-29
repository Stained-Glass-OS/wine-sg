#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml templated ScrollBar (patches/sg/6187): with a template of the program's (Windows
# Terminal's) a ScrollBar sizes its parts -- the thumb as long as the viewport is of the whole, the
# large decrease before it as the value puts it -- hides its other orientation's root, steps Value from
# its small and large RepeatButtons, moves it with its Thumb's drag, and goes to its states (Expanded
# with the mouse indicator). test/xamlscrollparts-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlscrollparts-gate.sh
# Mutants (dlls/windows.ui.xaml/range.c): SG_MUTANT_SCROLL_BAR_PARTS_STILL, SG_MUTANT_SCROLL_BAR_PARTS_INERT,
# SG_MUTANT_SCROLL_BAR_NOT_RELAID, SG_MUTANT_SCROLL_BAR_NO_STATES.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlscrollparts.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlscrollparts-probe.exe" "$HERE/xamlscrollparts-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/xamlscrollparts-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlscrollparts-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "a ScrollBar with a template of its own"
chk '^parts=80,0,1$' "the thumb half the track; nothing before it; the horizontal root collapsed"
chk '^half=40$' "Value 50: half the rest of the track before the thumb"
chk '^small=51$' "the small increase: one SmallChange"
chk '^large=61$' "the large increase: one LargeChange"
chk '^drag=70$' "the thumb dragged 16 pixels: 20 more"
chk '^collapsed=0$' "no indicator: Collapsed"
chk '^expanded=1$' "the mouse indicator: Expanded"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
