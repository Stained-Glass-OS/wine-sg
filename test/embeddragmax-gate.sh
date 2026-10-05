#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program drawing its own title bar (SG Office, Firefox with its tabs
# in the title bar, a GTK program's header bar), maximized in its Wine frame
# and dragged by its own title bar: it is restored under the pointer and
# follows it, as a maximized window dragged by its title bar does. It did
# nothing: the program asks to be moved (_NET_WM_MOVERESIZE) and the frame's
# move began -- but a maximized window is dragged out of it only with the
# left button down as Wine knows it, and the press had gone on to the
# program (wine-sg 0852). The program's own double-click on its title bar
# (Qt and GTK maximize and restore themselves so) keeps working.
#   1. maximized, its title bar pressed and dragged: its frame is restored,
#      at its size from before, under the pointer (the pointer as far across
#      the restored title bar, in proportion, as it was across the maximized
#      one, and as far below its top), and follows the pointer; released,
#      the pointer is free
#   2. maximized, a click on its title bar (no drag) leaves it maximized and
#      the pointer free
#   3. its own double-click on its title bar restores it, and another
#      maximizes it again; the pointer free after each
#   4. the button let go while the frame is being restored (held up 1.5 s
#      by a CBT hook): the move ends there -- the pointer stays Wine's while
#      it is restored (win32u drag_unmaximize); let go meanwhile, the release
#      went to the program and the frame followed the pointer, every click
#      the frame's, until another click
#
#   WINE=/opt/wine-sg/bin/wine test/embeddragmax-gate.sh
# Mutants: SG_MUTANT_EMBED_NO_DRAG_UNMAXIMIZE (dlls/winex11.drv/sg_embed.c)
# fails 1 (and 4); SG_MUTANT_DRAG_UNMAXIMIZE_UNHELD (dlls/win32u/defwnd.c)
# fails 4.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset DISPLAY XAUTHORITY WAYLAND_DISPLAY
for t in Xvfb xdotool xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-embeddragmax.XXXXXX); XP=; XC=; HK=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '[ -n "$HK" ] && kill "$HK" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; [ -n "$XC" ] && kill "$XC" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/ownframe-client" "$HERE/ownframe-client.c" -lX11 || { echo "FAIL  client did not build"; exit 1; }
cc -O2 -o "$T/grabcheck" "$HERE/embedmaxgrab-grabcheck.c" -lX11 || { echo "FAIL  grab check did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
"$MINGW" -O2 -shared -o "$T/slowrestore.dll" "$HERE/embeddragmax-slowrestore.c" &&
    "$MINGW" -O2 -DSLOWRESTORE_EXE -o "$T/slowrestore.exe" "$HERE/embeddragmax-slowrestore.c" || { echo "FAIL  hook did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
case "$DISPLAY" in :|:0) echo "FAIL  no display of our own"; exit 1 ;; esac
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$T/slowrestore.exe" "$T/slowrestore.dll" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
printf 'END\n' > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run" SG_FAKE_XWLOG="$T/xwlog"
WINEDEBUG="${EMBED_DEBUG:--all}" "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 4
cd "$WINEPREFIX/drive_c"

OWNFRAME_DBLCLICK=1 "$T/ownframe-client" "Own Frame" 200 150 400 300 & XC=$!
i=0; while [ -z "$(xwininfo -root -tree | awk '/"Own Frame"/ {print $1; exit}')" ] && [ $i -lt 100 ]; do sleep 0.1; i=$((i + 1)); done
XID=$(xwininfo -root -tree | awk '/"Own Frame"/ {print $1; exit}')
printf '%d shown - OwnFrame\tOwn Frame\nEND\n' "$XID" > "$T/list"
framed=; i=0
while [ $i -lt 300 ]; do
    p=$(xwininfo -id "$XID" -tree 2>/dev/null | sed -n 's/.*Parent window id: [^ ]* //p')
    case "$p" in ""|"(the root window)"*) ;; *) framed=1; break ;; esac
    sleep 0.05; i=$((i + 1))
done
[ -n "$framed" ] || { fail "never framed"; echo "RESULT: FAIL"; exit 1; }
sleep 1
probe() { "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r'; }
grab() { "$T/grabcheck" 2>/dev/null; }
zoomed() { probe zoomed SgLinuxWindow "Own Frame"; }
# wait for CMD's output to be WANT (Wine is slow on a loaded host)
waitfor() { want=$1; shift; k=0; while [ "$("$@")" != "$want" ] && [ $k -lt 20 ]; do sleep 0.25; k=$((k + 1)); done; }
probe activate SgLinuxWindow "Own Frame" >/dev/null; sleep 0.5
set -- $(probe rect SgLinuxWindow "Own Frame")
[ $# = 4 ] || { fail "no frame ($*)"; echo "RESULT: FAIL"; exit 1; }
W0=$(( $3 - $1 )) H0=$(( $4 - $2 ))

# 1. maximized, dragged by its own title bar
probe maximize SgLinuxWindow "Own Frame" >/dev/null; waitfor zoomed=1 zoomed
[ "$(zoomed)" = zoomed=1 ] || fail "its frame was not maximized"
set -- $(probe rect SgLinuxWindow "Own Frame")
ML=$1 MT=$2 MW=$(( $3 - $1 ))
SX=600 SY=$(( MT + 12 ))
xdotool mousemove $SX $SY; sleep 0.3; xdotool mousedown 1; sleep 0.4
for k in 1 2 3 4 5 6 7 8 9 10; do xdotool mousemove_relative -- 10 15; sleep 0.08; done
sleep 0.8
z=$(zoomed)
set -- $(probe rect SgLinuxWindow "Own Frame")
PX=$(( SX + 100 )) PY=$(( SY + 150 ))
if [ "$z" = zoomed=0 ] && [ $# = 4 ] && [ $(( $3 - $1 )) = "$W0" ] && [ $(( $4 - $2 )) = "$H0" ]; then
    # the pointer as far across, in proportion, and as far below the top
    want=$(( PX - (SX - ML) * W0 / MW ))
    dx=$(( $1 - want )); dy=$(( $2 - (PY - (SY - MT)) ))
    [ ${dx#-} -le 12 ] && [ ${dy#-} -le 12 ] \
        && pass "dragged by its title bar, maximized: restored (${W0}x$H0) under the pointer ($*; pointer $PX,$PY)" \
        || fail "restored, but not under the pointer: frame $*, pointer $PX,$PY (want left ~$want, top ~$(( PY - (SY - MT) )))"
    l1=$1 t1=$2
    for k in 1 2 3 4 5; do xdotool mousemove_relative -- -20 -10; sleep 0.08; done
    sleep 0.6
    set -- $(probe rect SgLinuxWindow "Own Frame")
    [ $# = 4 ] && [ $(( l1 - $1 )) -ge 80 ] && [ $(( t1 - $2 )) -ge 35 ] \
        && pass "...and it follows the pointer ($l1 $t1 -> $1 $2)" || fail "...it stopped following the pointer ($l1 $t1 -> $*)"
else
    fail "dragged by its title bar, maximized: $z, frame $* (want restored at ${W0}x$H0)"
fi
xdotool mouseup 1; sleep 1
waitfor pointer=free grab
g=$(grab); [ "$g" = "pointer=free" ] && pass "released, the pointer is free" || fail "released: $g"
set -- $(probe rect SgLinuxWindow "Own Frame"); r1="$*"
xdotool mousemove_relative -- 60 60; sleep 0.8
[ "$(probe rect SgLinuxWindow "Own Frame")" = "$r1" ] && pass "...and the frame stays where it was let go" \
    || fail "after the release the frame still moves ($r1 -> $(probe rect SgLinuxWindow "Own Frame"))"

# 2. maximized, a click on its title bar alone
probe maximize SgLinuxWindow "Own Frame" >/dev/null; waitfor zoomed=1 zoomed
set -- $(probe rect SgLinuxWindow "Own Frame")
xdotool mousemove 500 $(( $2 + 12 )); sleep 0.3; xdotool mousedown 1; sleep 0.25; xdotool mouseup 1; sleep 1.2
waitfor pointer=free grab
z=$(zoomed); g=$(grab)
[ "$z" = zoomed=1 ] && [ "$g" = pointer=free ] && pass "a click on its title bar leaves it maximized, the pointer free" \
    || fail "a click on its title bar: $z, $g"

# 3. its own double-click: restored, and maximized again
set -- $(probe rect SgLinuxWindow "Own Frame")
xdotool mousemove 500 $(( $2 + 12 )); sleep 0.3; xdotool click --repeat 2 --delay 120 1; sleep 0.3
waitfor zoomed=0 zoomed; sleep 0.5
z1=$(zoomed); waitfor pointer=free grab; g1=$(grab)
set -- $(probe rect SgLinuxWindow "Own Frame")
xdotool mousemove $(( $1 + 150 )) $(( $2 + 12 )); sleep 0.3; xdotool click --repeat 2 --delay 120 1; sleep 0.3
waitfor zoomed=1 zoomed; sleep 0.5
z2=$(zoomed); waitfor pointer=free grab; g2=$(grab)
[ "$z1" = zoomed=0 ] && [ "$z2" = zoomed=1 ] && [ "$g1" = pointer=free ] && [ "$g2" = pointer=free ] \
    && pass "its own double-click restores it, and maximizes it again (the pointer free)" \
    || fail "double-click: maximized -> $z1 ($g1), then -> $z2 ($g2)"

# 4. let go while it is being restored
"$WINE" slowrestore.exe > "$T/hook.out" 2>/dev/null & HK=$!
k=0; while ! grep -q 'hook=' "$T/hook.out" 2>/dev/null && [ $k -lt 40 ]; do sleep 0.25; k=$((k + 1)); done
if grep -q 'hook=1' "$T/hook.out"; then
    [ "$(zoomed)" = zoomed=1 ] || { probe maximize SgLinuxWindow "Own Frame" >/dev/null; waitfor zoomed=1 zoomed; }
    set -- $(probe rect SgLinuxWindow "Own Frame")
    xdotool mousemove 400 $(( $2 + 12 )); sleep 0.3; xdotool mousedown 1; sleep 0.4
    for k in 1 2 3 4; do xdotool mousemove_relative -- 10 15; sleep 0.08; done
    sleep 0.4; xdotool mouseup 1; sleep 2.5
    waitfor pointer=free grab
    z=$(zoomed); g=$(grab); r1=$(probe rect SgLinuxWindow "Own Frame")
    xdotool mousemove_relative -- 80 60; sleep 0.8; r2=$(probe rect SgLinuxWindow "Own Frame")
    [ "$z" = zoomed=0 ] && [ "$g" = pointer=free ] && [ "$r1" = "$r2" ] \
        && pass "let go while it was being restored: restored, the move over, the pointer free ($r1)" \
        || fail "let go while it was being restored: $z, $g, the frame $r1 -> $r2 after the pointer moved (want it to stay)"
    kill "$HK" 2>/dev/null; HK=
    xdotool click 1; sleep 0.5
else
    fail "the CBT hook was not set ($(cat "$T/hook.out"))"
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
