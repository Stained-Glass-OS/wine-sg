#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The children of a window with WS_EX_COMPOSITED paint bottom to top (6100). Windows paints all of a
# composited window's descendants into one buffer, lowest first; Wine's server handed out WM_PAINTs top
# first, so a lower sibling that does not clip its siblings painted over a higher one. Opera's uninstall
# dialog (RootScreen, composited) showed no Uninstall or Re-install button and no close button: its
# UninstallScreen covered the ButtonStrip above it. The probe: a composited window, a gray child over all
# of it (the bottom) and a red child over its lower third (the top).
#   WINE=/opt/wine-sg/bin/wine test/composited-gate.sh   (SG_XDISPLAY=N to use Xvfb :N)
# Mutant: SG_MUTANT_COMPOSITED_TOP_DOWN (server window.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in Xvfb import convert $MINGW; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
T=$(mktemp -d /var/tmp/sg-composited.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
unset DISPLAY XAUTHORITY
XP=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${ARTIFACTS:-}" ] && cp "$T"/*.png "$ARTIFACTS"/ 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -mwindows -o "$T/probe.exe" "$HERE/composited-probe.c" -lgdi32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
if [ -n "${SG_XDISPLAY:-}" ]; then
    Xvfb ":$SG_XDISPLAY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$SG_XDISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
sleep 8
"$WINE" explorer /desktop=shell,1024x700 'C:\probe.exe' 'C:\probe.out' >/dev/null 2>&1 &
O="$WINEPREFIX/drive_c/probe.out"
px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null | tail -n 1; }
i=0; while ! grep -q ready=1 "$O" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
grep -q ready=1 "$O" 2>/dev/null || { fail "the probe did not show its window"; echo "RESULT: FAIL"; exit 1; }
import -window root "$T/shown.png"
top=$(px shown 400 250); low=$(px shown 400 400)
[ "$top" = "128,128,128" ] && pass "the lower child shows where nothing is over it ($top)" || fail "upper part: $top, not gray"
[ "$low" = "255,0,0" ] && pass "the child on top shows over it when the window is first painted ($low)" \
    || fail "the top child is painted over by the one below it: $low, not red"
i=0; while ! grep -q redrawn=1 "$O" 2>/dev/null && [ $i -lt 30 ]; do sleep 0.5; i=$((i + 1)); done
sleep 1
import -window root "$T/redrawn.png"
low=$(px redrawn 400 400)
[ "$low" = "255,0,0" ] && pass "and after RedrawWindow with RDW_UPDATENOW ($low)" || fail "after RedrawWindow: $low, not red"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
