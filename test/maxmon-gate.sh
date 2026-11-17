#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A window maximized on the second monitor fills the second monitor (7818).
# David 2026-11-16: a Linux program maximized on the second screen ended up on
# the first. Its frame (programs/explorer/systray.c, a program drawing its own
# title bar) answers WM_GETMINMAXINFO with its maximized position as Windows
# programs do -- the top left of the work area, relative to the monitor -- and
# win32u took that as a point on the screen: 0,0, the first monitor. On
# Windows the position is the primary monitor's and the window manager moves
# it to the monitor the window is on.
#
# A headless sg-compositor with two outputs (1280x720 side by side), the
# shell's desktop over both (HostMonitors), a Linux window that draws its own
# title bar framed in it:
#   1. on the second monitor, maximized: its frame fills the second monitor
#   2. restored: back where it was
#   3. on the first monitor, maximized: it fills the first one
#
#   WINE=/opt/wine-sg/bin/wine test/maxmon-gate.sh
#   SG_COMPOSITOR_BUILD=<dir with sg-compositor>, or SG_COMPOSITOR_SRC=<checkout> to build one
# Mutant: SG_MUTANT_MAXPOS_ABSOLUTE (dlls/win32u/window.c) fails 1.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
SRC="${SG_COMPOSITOR_SRC:-$HERE/../../sg-compositor}"
[ -d "$SRC" ] || SRC="${SG_REAL_HOME:-$HOME}/Stained-Glass-OS/sg-compositor"
RC=0; CP=; XC=
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-maxmon.XXXXXX)
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "$XC" ] && kill "$XC" 2>/dev/null
    [ -n "$CP" ] && { pkill -P "$CP" 2>/dev/null; kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }
    [ -n "${KEEP:-}" ] && echo "kept $T" || rm -rf "$T"
}
trap cleanup EXIT INT TERM
if [ -n "${SG_COMPOSITOR_BUILD:-}" ] && [ -x "$SG_COMPOSITOR_BUILD/sg-compositor" ]; then
    COMP=$SG_COMPOSITOR_BUILD/sg-compositor
elif [ -d "$SRC" ]; then
    meson setup "$T/comp" "$SRC" -Dman-pages=disabled --buildtype=release >"$T/meson.log" 2>&1 &&
        ninja -C "$T/comp" >"$T/ninja.log" 2>&1 || { tail -20 "$T/meson.log" "$T/ninja.log"; echo "SKIP: cannot build the compositor"; exit 77; }
    COMP=$T/comp/sg-compositor
else
    echo "SKIP: no sg-compositor (SG_COMPOSITOR_BUILD or SG_COMPOSITOR_SRC)"; exit 77
fi
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/ownframe-client" "$HERE/ownframe-client.c" -lX11 || { echo "FAIL  client did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }

unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
mkdir -p "$T/prefix" "$T/comp-xdg" "$T/run"; chmod 700 "$T/comp-xdg" "$T/run"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER
XDG_RUNTIME_DIR="$T/comp-xdg" WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman \
    "$COMP" -L "$T/priv.sock" -C "$T/ctl.sock" -U "$(id -u)" -- \
    sh -c "echo \$DISPLAY > $T/disp; exec sleep 1200" >>"$T/comp.log" 2>&1 &
CP=$!
i=0; while [ ! -s "$T/disp" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
DISPLAY=$(cat "$T/disp" 2>/dev/null); export DISPLAY
case "$DISPLAY" in ""|:0) echo "FAIL  no display from the compositor"; cat "$T/comp.log"; exit 1 ;; esac
export XDG_RUNTIME_DIR="$T/run"
sleep 1
case "$(xwininfo -root 2>/dev/null | awk '/Width:/ {w=$2} /Height:/ {h=$2} END {print w "x" h}')" in
    2560x720) ;;
    *) echo "SKIP: the compositor did not make two 1280x720 outputs"; exit 77 ;;
esac
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d x11 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v HostMonitors /d Y /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
printf 'END\n' > "$T/list"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" SG_FAKE_XWLOG="$T/xwlog"
"$WINE" explorer /desktop=shell,2560x720 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 4
cd "$WINEPREFIX/drive_c"
probe() { "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r' | grep -v '^done$'; }
zoomed() { probe zoomed SgLinuxWindow "$TITLE"; }
waitfor() { want=$1; shift; k=0; while [ "$("$@")" != "$want" ] && [ $k -lt 20 ]; do sleep 0.25; k=$((k + 1)); done; }

# frame X TITLE: a Linux window TITLE at X (screen), framed; its frame's rect in $FR
frame() {
    TITLE=$2
    [ -n "$XC" ] && { kill "$XC" 2>/dev/null; wait "$XC" 2>/dev/null; XC=; printf 'END\n' > "$T/list"; sleep 1; }
    "$T/ownframe-client" "$TITLE" "$1" 150 400 300 & XC=$!
    i=0; while [ -z "$(xwininfo -root -tree | awk -v t="$TITLE" 'index($0, "\"" t "\"") {print $1; exit}')" ] && [ $i -lt 100 ]; do sleep 0.1; i=$((i + 1)); done
    XID=$(xwininfo -root -tree | awk -v t="$TITLE" 'index($0, "\"" t "\"") {print $1; exit}')
    printf '%d shown - OwnFrame\t%s\nEND\n' "$XID" "$TITLE" > "$T/list"
    i=0; FR=
    while [ $i -lt 40 ]; do FR=$(probe rect SgLinuxWindow "$TITLE"); [ -n "$FR" ] && break; sleep 0.25; i=$((i + 1)); done
    sleep 1; FR=$(probe rect SgLinuxWindow "$TITLE")
}

# 1, 2: on the second monitor
frame 1600 'Second Screen'
set -- $FR
if [ $# = 4 ] && [ "$1" -ge 1280 ]; then
    before=$FR
    probe maximize SgLinuxWindow "$TITLE" >/dev/null; waitfor zoomed=1 zoomed; sleep 0.5
    set -- $(probe rect SgLinuxWindow "$TITLE")
    [ $# = 4 ] && [ "$1" = 1280 ] && [ "$3" = 2560 ] && [ "$2" -ge 0 ] && [ "$4" -le 720 ] && [ $(( $4 - $2 )) -ge 600 ] \
        && pass "on the second monitor, maximized: it fills the second monitor ($*)" \
        || fail "on the second monitor ($before), maximized: $* (want 1280 0 2560 ~680)"
    probe restore SgLinuxWindow "$TITLE" >/dev/null; waitfor zoomed=0 zoomed; sleep 0.5
    [ "$(probe rect SgLinuxWindow "$TITLE")" = "$before" ] && pass "restored: back where it was ($before)" \
        || fail "restored: $(probe rect SgLinuxWindow "$TITLE") (want $before)"
else
    fail "the frame is not on the second monitor: '$FR'"
fi

# 3: on the first monitor (moved there: the shell puts a program's new
# frame where its last one was)
frame 200 'First Screen'
# (on a loaded host the frame can still be settling: move until it is there)
k=0
while [ $k -lt 10 ]; do
    probe move SgLinuxWindow "$TITLE" 200 150 400 300 >/dev/null; sleep 1
    FR=$(probe rect SgLinuxWindow "$TITLE")
    set -- $FR
    [ $# = 4 ] && [ "$3" -le 1280 ] && break
    k=$((k + 1))
done
set -- $FR
if [ $# = 4 ] && [ "$3" -le 1280 ]; then
    probe maximize SgLinuxWindow "$TITLE" >/dev/null; waitfor zoomed=1 zoomed; sleep 0.5
    set -- $(probe rect SgLinuxWindow "$TITLE")
    [ $# = 4 ] && [ "$1" = 0 ] && [ "$3" = 1280 ] && [ $(( $4 - $2 )) -ge 600 ] \
        && pass "on the first monitor, maximized: it fills the first monitor ($*)" \
        || fail "on the first monitor, maximized: $* (want 0 0 1280 ~680)"
else
    fail "the frame is not on the first monitor: '$FR'"
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
