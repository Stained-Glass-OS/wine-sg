#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program drawing its own title bar (SG Office), maximized in its
# Wine frame, clicked on its own title bar: the program asks to be moved
# (_NET_WM_MOVERESIZE), and the frame grabs the pointer for Wine's move loop
# -- which never starts for a maximized window. The grab stayed: every click
# went to the frame, the Start button and every other window dead, until
# Alt+F4 (David 2026-10-05, SG Office maximized and restored a few times).
# wine-sg 0818:
#   1. after a click on the maximized program's own title bar, another
#      client can take the pointer (no grab left behind)
#   2. a click elsewhere then reaches the desktop (a window behind it is
#      activated, not the frame)
#   3. restored, a drag on its title bar still moves the frame (0803's move,
#      whose loop has the grab and lets it go)
#
#   WINE=/opt/wine-sg/bin/wine test/embedmaxgrab-gate.sh
# Mutant: SG_MUTANT_EMBED_MOVESIZE_GRAB_LEAK (dlls/winex11.drv/sg_embed.c)
# fails 1 and 2.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset DISPLAY XAUTHORITY WAYLAND_DISPLAY
for t in Xvfb xdotool xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-embedmaxgrab.XXXXXX); XP=; XC=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XC" ] && kill "$XC" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/ownframe-client" "$HERE/ownframe-client.c" -lX11 || { echo "FAIL  client did not build"; exit 1; }
cc -O2 -o "$T/grabcheck" "$HERE/embedmaxgrab-grabcheck.c" -lX11 || { echo "FAIL  grab check did not build"; exit 1; }
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

"$T/ownframe-client" "Own Frame" 200 150 400 300 & XC=$!
i=0; while [ -z "$(xwininfo -root -tree | awk '/"Own Frame"/ {print $1; exit}')" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
XID=$(xwininfo -root -tree | awk '/"Own Frame"/ {print $1; exit}')
printf '%d shown - OwnFrame\tOwn Frame\nEND\n' "$XID" > "$T/list"
framed=; i=0
while [ $i -lt 120 ]; do
    p=$(xwininfo -id "$XID" -tree 2>/dev/null | sed -n 's/.*Parent window id: [^ ]* //p')
    case "$p" in ""|"(the root window)"*) ;; *) framed=1; break ;; esac
    sleep 0.05; i=$((i + 1))
done
[ -n "$framed" ] || { fail "never framed"; echo "RESULT: FAIL"; exit 1; }
sleep 1
grab() { "$T/grabcheck" 2>/dev/null; }
[ "$(grab)" = "pointer=free" ] || { fail "the pointer is grabbed before anything was clicked: $(grab)"; echo "RESULT: FAIL"; exit 1; }

# 1. maximized, clicked on its own title bar (held a moment, as a hand does)
"$WINE" probe.exe maximize SgLinuxWindow "Own Frame" >/dev/null 2>&1; sleep 1
z=$("$WINE" probe.exe zoomed SgLinuxWindow "Own Frame" 2>/dev/null | tr -d '\r')
[ "$z" = "zoomed=1" ] || fail "its frame was not maximized ($z)"
"$WINE" probe.exe rect SgLinuxWindow "Own Frame" 2>/dev/null | tr -d '\r' > "$T/rmax"
set -- $(cat "$T/rmax")
if [ $# = 4 ]; then
    xdotool mousemove $(( $1 + 300 )) $(( $2 + 12 )); sleep 0.3
    xdotool mousedown 1; sleep 0.25; xdotool mouseup 1; sleep 1.5
    g=$(grab)
    [ "$g" = "pointer=free" ] && pass "after a click on the maximized program's own title bar the pointer is free" \
        || fail "after a click on the maximized program's own title bar: $g (every click goes to the frame)"
    # 2. restored, and a click on the desktop beside it: the frame stops
    #    being the window in front (with the grab left, the click was the
    #    frame's)
    "$WINE" probe.exe restore SgLinuxWindow "Own Frame" >/dev/null 2>&1; sleep 1
    "$WINE" probe.exe move SgLinuxWindow "Own Frame" 500 300 400 300 >/dev/null 2>&1; sleep 0.5
    "$WINE" probe.exe activate SgLinuxWindow "Own Frame" >/dev/null 2>&1; sleep 0.5
    xdotool mousemove 50 50; sleep 0.3; xdotool click 1; sleep 1
    f=$("$WINE" probe.exe foreground SgLinuxWindow "Own Frame" 2>/dev/null | tr -d '\r')
    [ "$f" = "foreground=0" ] && pass "a click on the desktop then reaches the desktop (the frame is no longer in front)" \
        || fail "a click on the desktop went to the frame ($f)"
else
    fail "no frame to click ($(cat "$T/rmax"))"
fi

# 3. restored, its title bar dragged: the frame moves, and the pointer is free after
"$WINE" probe.exe rect SgLinuxWindow "Own Frame" 2>/dev/null | tr -d '\r' > "$T/r0"
set -- $(cat "$T/r0")
if [ $# = 4 ]; then
    l=$1 t=$2
    xdotool mousemove $(( l + 150 )) $(( t + 12 )); sleep 0.3; xdotool mousedown 1; sleep 0.7
    for k in 1 2 3 4 5 6; do xdotool mousemove_relative -- -20 10; sleep 0.1; done
    sleep 0.3; xdotool mouseup 1; sleep 1.2
    "$WINE" probe.exe rect SgLinuxWindow "Own Frame" 2>/dev/null | tr -d '\r' > "$T/r1"
    set -- $(cat "$T/r1")
    [ $# = 4 ] && [ $(( l - $1 )) -ge 100 ] && [ $(( $2 - t )) -ge 45 ] \
        && pass "restored, dragging its own title bar moves the frame ($(cat "$T/r0") -> $(cat "$T/r1"))" \
        || fail "drag: $(cat "$T/r0") -> $(cat "$T/r1") (want moved about -120,60)"
    g=$(grab)
    [ "$g" = "pointer=free" ] && pass "and the pointer is free after the drag" || fail "after the drag: $g"
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
