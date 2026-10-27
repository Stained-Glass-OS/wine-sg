#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml access keys (patches/sg/6163): Alt alone enters display mode with a key tip per
# AccessKey (AccessKeyManager.IsDisplayModeEnabled), a key typed then or Alt+key invokes its element (a
# button clicks, a text box takes the keyboard focus), a key of two letters waits for the second,
# Escape and Alt leave, an AccessKeyInvoked handler setting Handled keeps the click.
# test/xamlaccesskeys-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlaccesskeys-gate.sh
# Mutant (dlls/windows.ui.xaml/focus.c): SG_MUTANT_XAML_NO_ACCESS_KEYS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlaccesskeys.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlaccesskeys-probe.exe" "$HERE/xamlaccesskeys-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlaccesskeys-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlaccesskeys-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup with AccessKeys"
chk '^key=S,1$' "UIElement.AccessKey; AccessKeyManager"
chk '^display=1,1$' "Alt alone: display mode, key tips drawn"
chk '^typed=1,0,0$' "a key typed invokes its button (Click), display mode ends"
chk '^altkey=1,0$' "Alt+key invokes at once"
chk '^textbox=2,0$' "a text box's key gives it the keyboard focus"
chk '^twokeys=1,1,0$' "a key of two letters: the first waits, the second invokes"
chk '^leave=0,0,0$' "Escape leaves display mode, Alt twice enters and leaves"
chk '^handled=1,0$' "AccessKeyInvoked handled: no Click"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
