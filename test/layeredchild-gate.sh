#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A layered child window (6106). JetBrains' runtime (PyCharm, IntelliJ IDEA) draws its title bar's
# minimise, maximise and close buttons in a WS_EX_LAYERED child filled by UpdateLayeredWindow. winex11
# made that child's X window at the root with the child's parent-relative position: the buttons showed
# near the screen's top edge, stayed there when the window moved, and the frame's paint covered the
# place they belong. Now the child's X window sits in its top-level's, at its place, the frame's paint
# goes around it, and the pointer goes through it to the frame (Win32 hit-testing picks the child).
# The probe (test/layeredchild-probe.c): a blue popup at 200,150 with a red 80x40 layered child at
# 300,0, then moved to 100,300.
#   WINE=/opt/wine-sg/bin/wine test/layeredchild-gate.sh
# Mutant: SG_MUTANT_LAYERED_CHILD_ROOT (winex11 window.c).
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
for t in Xvfb xdotool import convert $MINGW; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
T=$(mktemp -d /var/tmp/sg-layeredchild.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
unset DISPLAY XAUTHORITY
XP=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${ARTIFACTS:-}" ] && cp "$T"/*.png "$ARTIFACTS"/ 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -mwindows -o "$T/probe.exe" "$HERE/layeredchild-probe.c" 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
sleep 8
"$WINE" explorer /desktop=shell,1024x700 'C:\probe.exe' 'C:\probe.out' >/dev/null 2>&1 &
wait_for() { i=0; while ! grep -q "$1" "$WINEPREFIX/drive_c/probe.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done; }
v() { tr -d '\r' < "$WINEPREFIX/drive_c/probe.out" | sed -n "s/^$1=//p" | tail -n 1; }
px() { convert "$1" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null | tail -n 1; }
centre() { r=$(v "$1"); x=$(( ($(echo "$r" | cut -d, -f1) + $(echo "$r" | cut -d, -f3)) / 2 )); y=$(( ($(echo "$r" | cut -d, -f2) + $(echo "$r" | cut -d, -f4)) / 2 )); }

wait_for ready=1
sleep 1
[ "$(v ulw)" = 1 ] && pass "UpdateLayeredWindow on the child succeeds" || fail "ulw: $(v ulw)"
[ -n "$(v child)" ] || { fail "the probe did not report"; echo "RESULT: FAIL"; exit 1; }
import -window root "$T/shot1.png"
centre child
p=$(px "$T/shot1.png" $x $y)
[ "$p" = "255,0,0" ] && pass "the child shows at its place in the frame ($x,$y: $p)" || fail "child at $x,$y: $p"
p=$(px "$T/shot1.png" 340 20)
[ "$p" != "255,0,0" ] && pass "nothing of it at its parent-relative place on the screen (340,20: $p)" || fail "red at 340,20"
p=$(px "$T/shot1.png" 250 200)
[ "$p" = "0,0,255" ] && pass "the frame's own paint shows ($p)" || fail "frame at 250,200: $p"

wait_for moved_ready=1
sleep 1
import -window root "$T/shot2.png"
centre moved
p=$(px "$T/shot2.png" $x $y)
[ "$p" = "255,0,0" ] && pass "it moves with the frame ($x,$y: $p)" || fail "moved child at $x,$y: $p"
xdotool mousemove $x $y; sleep 0.4; xdotool mousedown 1; sleep 0.2; xdotool mouseup 1
wait_for frame_click=
c=$(v frame_click)
[ "$c" = "340,20" ] && pass "a click on the transparent child reaches the frame at the child's place ($c)" || fail "frame click: '$c'"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
