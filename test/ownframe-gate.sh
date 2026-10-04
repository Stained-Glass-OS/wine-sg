#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program that draws its own title bar (SG Office, Firefox with its
# tabs in the title bar) asks for no decorations (_MOTIF_WM_HINTS); in its
# Wine frame (0762) it had a second title bar above its own (David
# 2026-10-02: "SG Office has 2 title bars"). wine-sg 0803:
#   1. its frame has no title bar or border of its own: the frame is the
#      program's window's size
#   2. dragging the program's own title bar moves the frame
#      (_NET_WM_MOVERESIZE, as Qt and GTK ask a window manager), and its
#      corner sizes it
#   3. a new Linux window is framed soon after the list naming it is asked
#      for (the taskbar reads it 250 ms later, not a whole tick), so the
#      compositor's own title bar is not seen first
#
#   WINE=/opt/wine-sg/bin/wine test/ownframe-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset DISPLAY XAUTHORITY WAYLAND_DISPLAY
for t in Xvfb xdotool xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ownframe.XXXXXX); XP=; XC=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XC" ] && kill "$XC" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/ownframe-client" "$HERE/ownframe-client.c" -lX11 || { echo "FAIL  client did not build"; exit 1; }
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
now() { date +%s.%N | cut -c1-14; }
t_list=$(now)
printf '%d shown - OwnFrame\tOwn Frame\nEND\n' "$XID" > "$T/list"
framed=; i=0
while [ $i -lt 120 ]; do
    p=$(xwininfo -id "$XID" -tree 2>/dev/null | sed -n 's/.*Parent window id: [^ ]* //p')
    case "$p" in ""|"(the root window)"*) ;; *) framed=$(now); break ;; esac
    sleep 0.05; i=$((i + 1))
done
[ -n "$framed" ] && pass "the program's window is framed ($p)" || { fail "never framed"; echo "RESULT: FAIL"; exit 1; }
# 3. how soon after the list naming it was asked for
t_req=$(awk -v t="$t_list" '$1 >= t { print; exit }' "$T/xwlog" 2>/dev/null)
d=$(awk -v a="${t_req:-0}" -v b="$framed" 'BEGIN { printf "%.2f", b - a }')
awk -v d="$d" -v r="${t_req:-}" 'BEGIN { exit !(r != "" && d >= 0 && d < 0.75) }' \
    && pass "framed ${d} s after the list naming it was asked for" || fail "framed ${d} s after the list was asked for (${t_req:-never asked})"

# 1. no title bar of the frame's own
sleep 1
"$WINE" probe.exe rect SgLinuxWindow "Own Frame" 2>/dev/null | tr -d '\r' > "$T/rect0"
set -- $(cat "$T/rect0")
[ $# = 4 ] && [ $(( $3 - $1 )) = 400 ] && [ $(( $4 - $2 )) = 300 ] \
    && pass "its frame has no title bar or border of its own: $(( $3 - $1 ))x$(( $4 - $2 )), the program's size" \
    || fail "its frame: $(cat "$T/rect0") (want 400x300: the program's own size)"

# 2. its own title bar dragged: the frame moves
if [ $# = 4 ]; then
    l=$1 t=$2
    xdotool mousemove $(( l + 150 )) $(( t + 12 )); sleep 0.3; xdotool mousedown 1; sleep 0.7
    for k in 1 2 3 4 5 6; do xdotool mousemove_relative -- 20 10; sleep 0.1; done
    sleep 0.3; xdotool mouseup 1; sleep 1.2
    "$WINE" probe.exe rect SgLinuxWindow "Own Frame" 2>/dev/null | tr -d '\r' > "$T/rect1"
    set -- $(cat "$T/rect1")
    [ $# = 4 ] && [ $(( $1 - l )) -ge 100 ] && [ $(( $1 - l )) -le 140 ] && [ $(( $2 - t )) -ge 45 ] && [ $(( $2 - t )) -le 75 ] \
        && pass "dragging its own title bar moves the frame ($(cat "$T/rect0") -> $(cat "$T/rect1"))" \
        || fail "drag: $(cat "$T/rect0") -> $(cat "$T/rect1") (want moved about 120,60)"
    # its bottom right corner: sized
    if [ $# = 4 ]; then
        w0=$(( $3 - $1 )) h0=$(( $4 - $2 ))
        xdotool mousemove $(( $3 - 6 )) $(( $4 - 6 )); sleep 0.3; xdotool mousedown 1; sleep 0.7
        for k in 1 2 3 4; do xdotool mousemove_relative -- 15 10; sleep 0.1; done
        sleep 0.3; xdotool mouseup 1; sleep 1.2
        "$WINE" probe.exe rect SgLinuxWindow "Own Frame" 2>/dev/null | tr -d '\r' > "$T/rect2"
        set -- $(cat "$T/rect2")
        [ $# = 4 ] && [ $(( $3 - $1 - w0 )) -ge 40 ] && [ $(( $4 - $2 - h0 )) -ge 25 ] \
            && pass "and its corner sizes it ($(cat "$T/rect1") -> $(cat "$T/rect2"))" \
            || fail "size: $(cat "$T/rect1") -> $(cat "$T/rect2") (want about 60x40 larger)"
    fi
    # a quick click on its title bar (pressed and let go at once, as a tap
    # is): the frame must not then follow the pointer (David 2026-10-03:
    # GNOME Secrets' window stuck to the pointer, clicks or not)
    "$WINE" probe.exe rect SgLinuxWindow "Own Frame" 2>/dev/null | tr -d '\r' > "$T/rect3"
    set -- $(cat "$T/rect3")
    if [ $# = 4 ]; then
        xdotool mousemove $(( $1 + 150 )) $(( $2 + 12 )); sleep 0.3; xdotool click 1; sleep 0.8
        for k in 1 2 3 4 5 6; do xdotool mousemove_relative -- 25 15; sleep 0.1; done
        sleep 0.8
        "$WINE" probe.exe rect SgLinuxWindow "Own Frame" 2>/dev/null | tr -d '\r' > "$T/rect4"
        [ "$(cat "$T/rect4")" = "$(cat "$T/rect3")" ] \
            && pass "a quick click on its title bar leaves the frame where it is (it does not follow the pointer after)" \
            || fail "after a quick click the frame followed the pointer: $(cat "$T/rect3") -> $(cat "$T/rect4")"
        xdotool click 1; sleep 0.5    # (an old build's loop: end it)
    fi
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
