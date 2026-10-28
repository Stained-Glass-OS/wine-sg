#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml ListView selections (patches/sg/6167): Extended -- Down selects one, Shift+Down extends
# the range, Ctrl+Down moves alone, Ctrl+Space toggles; Multiple -- Space toggles, Down leaves the
# selection; SelectedItems kept, and followed when the application appends to it; SelectionChanged with
# AddedItems/RemovedItems; a horizontal ItemsStackPanel in one row with Left/Right. test/xamlmultiselect-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlmultiselect-gate.sh
# Mutants (dlls/windows.ui.xaml/textinput.c): SG_MUTANT_LIST_SINGLE_ONLY, SG_MUTANT_ITEMS_ALWAYS_VERTICAL.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlmultiselect.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlmultiselect-probe.exe" "$HERE/xamlmultiselect-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlmultiselect-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlmultiselect-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup with three lists"
chk '^extended=B BC BCD EBCD BCDE BCE E$' "Extended: Down, Shift+Down ranges, Ctrl+Down, Ctrl+Space toggles"
chk '^changed=1,0,2$' "SelectionChanged once per change (6), AddedItems/RemovedItems of the last change"
chk '^vector=EA,0$' "SelectedItems.Append by the application: the item selected (appended), SelectedIndex the first in the list"
chk '^multiple=X X X XY X$' "Multiple: Space toggles, Down leaves the selection"
chk '^row=PQQ,Q,1$' "a horizontal ItemsStackPanel: one row, Right moves and selects, Down stays"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
