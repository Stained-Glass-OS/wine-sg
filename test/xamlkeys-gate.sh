#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml keyboard (patches/sg/6153): a TextBox clicked takes the focus (GotFocus), typed
# characters and editing keys (Left, Back, Home, End, Delete) change its text, KeyDown routes up to
# its parents and a handler setting Handled keeps the box from using the key, Control.Focus moves the
# focus (LostFocus), PreviewKeyDown tunnels from the root first (patches/sg/6154) -- NanaZip's address
# bar takes a path. test/xamlkeys-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlkeys-gate.sh
# Mutant (dlls/windows.ui.xaml/textinput.c): SG_MUTANT_TEXTBOX_NO_KEYS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlkeys.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlkeys-probe.exe" "$HERE/xamlkeys-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlkeys-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlkeys-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup with two TextBoxes"
chk '^focus=1,1$' "a click gives the box the focus: GotFocus, FocusState"
chk '^typed=abc$' "a typed character"
chk '^caret=abXc$' "Left moves the caret"
chk '^back=abc$' "Back deletes before the caret"
chk '^routed=1,8$' "KeyDown routed up to the Grid (VK_BACK)"
chk '^handled=abZc$' "a KeyDown handler setting Handled keeps Home from the box (the caret stays)"
chk '^delete=abZ$' "End, Delete at the end, Left, Delete"
chk '^preview=1,0,abZ$' "PreviewKeyDown on the Grid handled first: no KeyDown, the box keeps its text"
chk '^moved=00000000,1,1,o$' "Control.Focus: the other box takes the keys, LostFocus"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
