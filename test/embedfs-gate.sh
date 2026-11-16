#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A framed Linux program's full-screen window goes, and its frame with it
# (David 2026-11-15: Enshrouded under Proton exited, and its SgLinuxWindow
# stayed mapped, full screen, black and empty, on top of everything; the
# shell then held the pointer -- most likely the stuck move loop that 7790
# ends; none of the ways below reproduces it on 7790 or before).
# A stand-in (embedfs-app.c) framed by the taskbar asks for full screen, then
# goes: destroying its window; ending its process; ending right after a click
# into it asked the frame to move it (the button still down), with and without
# the press's own grab kept; destroying it while the shell is stopped, a click
# landing on the frame before the shell hears of it. Checked each time: no
# frame of it left shown, no full-screen X window of the shell's, the pointer
# free (another client can take it); at the end the shell still runs.
#   WINE=/opt/wine-sg/bin/wine test/embedfs-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset SESSION_MANAGER
for t in Xvfb xwininfo import cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-embedfs.XXXXXX); XP=; AP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$AP" ] && kill "$AP" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/app" "$HERE/embedfs-app.c" -lX11 || { echo "FAIL  embedfs-app did not build"; exit 1; }
cc -O2 -o "$T/grabcheck" "$HERE/embedmaxgrab-grabcheck.c" -lX11 || { echo "FAIL  grab check did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
if [ -n "${EMBEDFS_DISPLAY:-}" ]; then
    Xvfb ":$EMBEDFS_DISPLAY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$EMBEDFS_DISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
printf 'END\n' > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
WINEDEBUG="${EMBED_DEBUG:--all}" "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 & EXPID=$!
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 4
cd "$WINEPREFIX/drive_c"
probe() { "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r'; }
root_id=$(xwininfo -root | awk '/Window id:/ {print $4; exit}')

for MODE in destroy exit moveexit keepexit stopped; do
    L="$T/app-$MODE.log"; : > "$L"
    if [ "$MODE" = stopped ]; then "$T/app" destroy 6 "$L" & AP=$!; else "$T/app" "$MODE" 4 "$L" & AP=$!; fi
    i=0; while ! grep -q '^main ' "$L" && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    XID=$(awk '$1 == "main" {print $2; exit}' "$L")
    printf '%d shown - Game\tGame\nEND\n' "$XID" > "$T/list"
    sleep 5.5   # framed, then full screen
    r=$(probe rect SgLinuxWindow Game)
    [ "$r" = "0 0 1024 700" ] || echo "NOTE  $MODE: its frame before it went: $r (expected full screen)"
    if [ "$MODE" = stopped ]; then
        # the shell busy as the window goes (a loaded machine, a game quitting): the window
        # is destroyed, and a click lands on the full-screen frame before the shell hears
        # of either. The frame's button grab froze the pointer for the shell; told the
        # window was gone first, it then let go of nothing -- every click dead, the shell's
        # thread in a move loop waiting for a release that could not come, the frame up
        kill -STOP "$EXPID"
        sleep 4
        xdotool mousemove 500 350 click 1 2>/dev/null
        sleep 0.5
        kill -CONT "$EXPID"
    fi
    if [ "$MODE" = moveexit ] || [ "$MODE" = keepexit ]; then
        # a press into it (it asks to be moved and ends), the button let go after it ended
        command -v xdotool >/dev/null && { xdotool mousemove 500 350 mousedown 1; sleep 0.3; xdotool mousemove 520 360; sleep 2.5; xdotool mouseup 1; }
    fi
    wait "$AP" 2>/dev/null; AP=
    printf 'END\n' > "$T/list"   # gone from the compositor's list too
    sleep 3
    left=$(probe list SgLinuxWindow - | grep -c visible)
    big=$(xwininfo -root -tree | awk '$0 ~ /explorer.exe/ && $0 ~ /1024x700\+0\+0/ && $0 !~ /Wine Desktop/' | grep -c .)
    g=$("$T/grabcheck" 2>/dev/null)
    [ "$left" = 0 ] && [ "$big" = 0 ] && pass "$MODE: its full-screen window gone, its frame went too" \
        || fail "$MODE: after it went, $left frame(s) still shown, $big full-screen X window(s) of the shell's: $(probe list SgLinuxWindow - | tr '\n' ';')"
    [ "$g" = pointer=free ] && pass "$MODE: and the pointer is free ($g)" || fail "$MODE: the pointer after it went: $g"
done
"$WINE" probe.exe exists Shell_TrayWnd "" | grep -q exists=1 && pass "the shell is still running" || fail "the shell ended"
[ $RC = 0 ] && echo "embedfs: all passed"
exit $RC
