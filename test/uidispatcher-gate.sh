#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI for XAML Islands (patches/sg/6175): the thread CoreWindow's CoreDispatcher (HasThreadAccess;
# RunAsync from its thread and from another runs the callback on its thread, the IAsyncAction completed
# then), Windows.UI.Colors and ColorHelper, AccessibilitySettings with a weak reference -- what Windows
# Terminal and WinUI 2 controls ask of it. test/uidispatcher-probe.c (the XAML switch on, for the
# thread's CoreWindow).
#   WINE=/opt/wine-sg/bin/wine test/uidispatcher-gate.sh
# Mutants (dlls/windows.ui): SG_MUTANT_DISPATCHER_NO_RUN (dispatcher.c), SG_MUTANT_COREWINDOW_NO_DISPATCHER
# (thread_corewindow.c), SG_MUTANT_COLORS_ALL_BLACK (colors.c), SG_MUTANT_ACCESSIBILITY_NO_WEAK (accessibility.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-uidispatcher.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
XP=""
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/uidispatcher-probe.c" -lruntimeobject -luser32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
mkdir -p "$WINEPREFIX"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
out=$(WINE_SG_XAML=1 timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qx "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'run=00000000,1,0' "RunAsync from the CoreWindow's thread: queued, not run at once; HasThreadAccess there"
chk 'ran=1,1,1,1' "the message loop runs it on that thread; its action Completed, its handler told"
chk 'other=00000000,0' "RunAsync from another thread is taken; HasThreadAccess is false there"
chk 'posted=1,1' "that callback runs on the CoreWindow's thread"
chk 'red=ffff0000' "Colors.Red"
chk 'transparent=00ffffff' "Colors.Transparent: white with no alpha"
chk 'cornflower=ff6495ed' "Colors.CornflowerBlue"
chk 'argb=80010203' "ColorHelper.FromArgb"
chk 'name=Red' "ColorHelper.ToDisplayName of a named colour"
chk 'accessibility=1,0,1,1' "AccessibilitySettings: made, no high contrast, a weak reference that resolves"
chk 'gone=1' "the weak reference resolves to nothing once it is gone"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
