#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml visual states and storyboards (patches/sg/6184): VisualStateManager.GoToState over a
# UserControl's VisualStateGroups from markup -- a state's Setters and its Storyboard's end values, as
# animated values above the local ones, taken back when the next state is entered; a composable
# VisualStateManager; a program's own Storyboard (SetTarget, SetTargetProperty, Begin, Completed, Stop);
# a DependencyObject's Dispatcher asked from another thread (Windows Terminal's render thread);
# ElementCompositionPreview.GetElementVisual; a KeySpline written "0,0,0,1" (6187). test/xamlstates-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlstates-gate.sh
# Mutants (dlls/windows.ui.xaml): SG_MUTANT_STATE_VALUES_NOT_SET, SG_MUTANT_VSM_NO_STATES,
# SG_MUTANT_STORYBOARD_NOT_BEGUN, SG_MUTANT_STORYBOARD_NO_COMPLETED (animation.c),
# SG_MUTANT_NO_ANIMATED_VALUES, SG_MUTANT_DISPATCHER_OF_CALLER (dependency.c),
# SG_MUTANT_VSM_NOT_COMPOSABLE (templates.c), SG_MUTANT_NO_COMPOSITOR (composition.c),
# SG_MUTANT_KEYSPLINE_SPACE_ONLY (animation.c, 6187).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlstates.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlstates-probe.exe" "$HERE/xamlstates-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
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
cp "$T/xamlstates-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlstates-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup with VisualStateManager.VisualStateGroups"
chk '^groups=1,1$' "GetVisualStateGroups"
chk '^wide=1,80,10,Wide$' "GoToState: a Setter's value; the group's CurrentState"
chk '^tall=1,20,33,Tall$' "the next state: the Setter taken back, the Storyboard's end value"
chk '^normal=1,20,10$' "an empty state: the local values again"
chk '^nostate=0$' "no such state"
chk '^spline=1,55$' "a key frame animation; KeySpline=\"0,0,0,1\" (6187)"
chk '^vsm_new=00000000,1$' "a composable VisualStateManager"
chk '^begun=00000000,0.25,1$' "Storyboard.Begin: the animation's end value, Completed"
chk '^stopped=1.00$' "Storyboard.Stop: the value before it"
chk '^dispatcher=1,1$' "Dispatcher from another thread: the one the element was made on"
chk '^visual=00000000,1$' "ElementCompositionPreview.GetElementVisual"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
