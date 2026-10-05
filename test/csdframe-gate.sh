#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program whose window draws its own title bar AND its shadow --
# GTK's client-side decorations (GNOME apps, GTK 4) -- names the shadow's
# width around it in _GTK_FRAME_EXTENTS; a window manager keeps that outside
# the window's frame. Our frame (0803) took the shadow for part of the
# window: the frame was the shadow's size larger, the shadow drawn inside it
# on its own background, the title bar it drags inset. wine-sg 0813:
#   1. the frame is the window's visible size (its content), not its shadow's
#   2. no shadow shows: it is outside the frame (clipped)
#   3. its own title bar, at the frame's top, moves the frame; its corner,
#      at the frame's corner, sizes it
#      and the frame's own edge sizes it (GTK's handles are in the shadow)
#   4. maximized, the program drops its shadow (GTK does): it fills the frame
#      at once; restored, the shadow is back outside the frame again
#
#   WINE=/opt/wine-sg/bin/wine test/csdframe-gate.sh   (mutants SG_MUTANT_EMBED_CSD_SHADOW_IN, SG_MUTANT_EMBED_CSD_NO_EDGES)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset DISPLAY XAUTHORITY WAYLAND_DISPLAY
for t in Xvfb xdotool xwininfo xwd convert python3 cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-csdframe.XXXXXX); XP=; XC=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XC" ] && kill "$XC" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/csdframe-client" "$HERE/csdframe-client.c" -lX11 || { echo "FAIL  client did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
case "$DISPLAY" in :|:0) echo "FAIL  no display of our own"; exit 1 ;; esac
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
printf 'END\n' > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run" SG_FAKE_XWLOG="$T/xwlog"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
sleep 8
cd "$WINEPREFIX/drive_c"

"$T/csdframe-client" "Csd Frame" 200 150 400 300 20 20 15 25 & XC=$!
i=0; while [ -z "$(xwininfo -root -tree | awk '/"Csd Frame"/ {print $1; exit}')" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
XID=$(xwininfo -root -tree | awk '/"Csd Frame"/ {print $1; exit}')
printf '%d shown - CsdFrame\tCsd Frame\nEND\n' "$XID" > "$T/list"
framed=; i=0
while [ $i -lt 120 ]; do
    p=$(xwininfo -id "$XID" -tree 2>/dev/null | sed -n 's/.*Parent window id: [^ ]* //p')
    case "$p" in ""|"(the root window)"*) ;; *) framed=1; break ;; esac
    sleep 0.05; i=$((i + 1))
done
[ -n "$framed" ] && pass "the program's window is framed ($p)" || { fail "never framed"; echo "RESULT: FAIL"; exit 1; }
sleep 1.5
rect() { "$WINE" probe.exe rect SgLinuxWindow "Csd Frame" 2>/dev/null | tr -d '\r'; }
# magenta pixels on the screen: the shadow seen
magenta() {
    xwd -root -silent | convert xwd:- -depth 8 rgb:- | python3 -c '
import sys
px = sys.stdin.buffer.read()
n = 0
for i in range(0, len(px) - 2, 3):
    if px[i] > 200 and px[i + 1] < 60 and px[i + 2] > 200: n += 1
print(n)'
}
r0=$(rect); set -- $r0
[ $# = 4 ] && [ $(( $3 - $1 )) = 400 ] && [ $(( $4 - $2 )) = 300 ] \
    && pass "its frame is its visible size, 400x300 (the shadow, 20 20 15 25, outside it)" \
    || fail "its frame: $r0 (want 400x300; the shadow would make it 440x340)"
m=$(magenta); [ "${m:-1}" = 0 ] && pass "no shadow shows (it is outside the frame)" || fail "the shadow shows: $m magenta pixels"

if [ $# = 4 ]; then
    l=$1 t=$2
    xdotool mousemove $(( l + 150 )) $(( t + 12 )); sleep 0.3; xdotool mousedown 1; sleep 0.7
    for k in 1 2 3 4 5 6; do xdotool mousemove_relative -- 20 10; sleep 0.1; done
    sleep 0.3; xdotool mouseup 1; sleep 1.2
    r1=$(rect); set -- $r1
    [ $# = 4 ] && [ $(( $1 - l )) -ge 100 ] && [ $(( $1 - l )) -le 140 ] && [ $(( $2 - t )) -ge 45 ] && [ $(( $2 - t )) -le 75 ] \
        && pass "its own title bar, at the frame's top, moves the frame ($r0 -> $r1)" \
        || fail "drag at the frame's top: $r0 -> $r1 (want moved about 120,60)"
    if [ $# = 4 ]; then
        w0=$(( $3 - $1 )) h0=$(( $4 - $2 ))
        xdotool mousemove $(( $3 - 6 )) $(( $4 - 6 )); sleep 0.3; xdotool mousedown 1; sleep 0.7
        for k in 1 2 3 4; do xdotool mousemove_relative -- 15 10; sleep 0.1; done
        sleep 0.3; xdotool mouseup 1; sleep 1.2
        r2=$(rect); set -- $r2
        [ $# = 4 ] && [ $(( $3 - $1 - w0 )) -ge 40 ] && [ $(( $4 - $2 - h0 )) -ge 25 ] \
            && pass "its corner, at the frame's corner, sizes it ($r1 -> $r2)" \
            || fail "size at the frame's corner: $r1 -> $r2 (want about 60x40 larger)"
        m=$(magenta); [ "${m:-1}" = 0 ] && pass "sized, still no shadow inside the frame" || fail "after sizing the shadow shows: $m magenta pixels"
        # its left edge, where GTK's own handle is in the (clipped) shadow: the
        # frame's edge sizes it
        r2=$(rect); set -- $r2
        if [ $# = 4 ]; then
            l2=$1 w2=$(( $3 - $1 ))
            xdotool mousemove $(( $1 + 2 )) $(( ($2 + $4) / 2 )); sleep 0.3; xdotool mousedown 1; sleep 0.7
            for k in 1 2 3 4; do xdotool mousemove_relative -- -10 0; sleep 0.1; done
            sleep 0.3; xdotool mouseup 1; sleep 1.2
            r3=$(rect); set -- $r3
            [ $# = 4 ] && [ $(( l2 - $1 )) -ge 30 ] && [ $(( $3 - $1 - w2 )) -ge 30 ] \
                && pass "its left edge (GTK's handle is in the shadow) sizes the frame ($r2 -> $r3)" \
                || fail "left edge: $r2 -> $r3 (want about 40 wider to the left)"
        fi
    fi
fi

"$WINE" probe.exe maximize SgLinuxWindow "Csd Frame" >/dev/null 2>&1; sleep 2
m=$(magenta)
g=$(xwininfo -id "$XID" | awk '/Relative upper-left X/ {x=$NF} /Relative upper-left Y/ {y=$NF} END {print x "," y}')
[ "${m:-1}" = 0 ] && [ "$g" = "0,0" ] && pass "maximized, it drops its shadow and fills the frame (at 0,0 in it)" \
    || fail "maximized: $m magenta pixels, the program at $g in the frame (want 0 and 0,0)"
"$WINE" probe.exe restore SgLinuxWindow "Csd Frame" >/dev/null 2>&1; sleep 2
m=$(magenta)
[ "${m:-1}" = 0 ] && pass "restored, its shadow is back outside the frame" || fail "restored: the shadow shows: $m magenta pixels"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
