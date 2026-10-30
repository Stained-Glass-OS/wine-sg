#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml pointer routed events (patches/sg/6213): mouse messages to an island raise
# PointerEntered/Moved/Pressed/Released on the element under the pointer, bubbling up while not
# handled; handlers added with handledEventsToo still see them; GetCurrentPoint is relative to the
# element asked, with the buttons held and what changed. React Native for Windows (OneDrive's Sign
# in button) takes the pointer from them. TextBox raises TextChanging and SelectionChanged too (React
# Native's text input reads its text on TextChanging). test/xamlpointer-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlpointer-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_NO_POINTER_EVENTS (pointer.c), SG_MUTANT_ADD_HANDLER_BOXED (element.c),
# SG_MUTANT_NO_TEXT_CHANGING, SG_MUTANT_NO_SELECTION_CHANGED (textinput.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlpointer.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlpointer-probe.exe" "$HERE/xamlpointer-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-264}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$T/xamlpointer-probe.exe" "$C/"
python3 "$HERE/mkpri.py" "$C/resources.pri" || { echo "FAIL  resources.pri not written"; exit 1; }
python3 "$HERE/mkxbf.py" "$C" || { echo "FAIL  fixtures not written"; exit 1; }
mkdir -p "$C/Lib"
printf '%s' '<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><TextBlock x:Name="T" x:Uid="LibUid" Text="markup"/></Grid>' > "$C/Lib/LibPage.xaml"
out=$(cd "$C" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlpointer-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^add_handler=00000000$' "AddHandler(PointerPressedEvent, handledEventsToo)"
chk '^moved=1 entered=1 at=50,40$' "PointerEntered, PointerMoved relative to the element"
chk '^pressed=1 at=50,40 left=1 kind=1 id=1 type=2 root=1$' "PointerPressed: left button, LeftButtonPressed, the mouse; it bubbles to the root"
chk '^released=1 kind=2 left=0$' "PointerReleased: LeftButtonReleased"
chk '^add_boxed=00000000$' "AddHandler with a boxed delegate (C++/WinRT box_value)"
chk '^boxed=1 at=50,40$' "the boxed delegate's handler is called, not the box"
chk '^handled=eCGrx inner_at=10,5$' "handled by the inner element: its parent does not see it, a handledEventsToo handler does"
chk '^typed=123,1,1$' "typing: TextChanging (IsContentChanging) before TextChanged, then SelectionChanged"
chk '^selected=3$' "TextBox.Select: SelectionChanged"
chk '^done=1$' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
