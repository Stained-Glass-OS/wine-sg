#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program's window goes into a Wine window of its own (wine-sg
# patches/sg/0762, winex11 sg_embed.c): it stacks among Wine's windows and is
# activated, minimized and closed as they are (David 2026-10-02). A real
# xterm, listed to the taskbar by a stand-in sg-lockctl as the compositor
# lists it:
#   1. it is put into the taskbar's SgLinuxWindow (its X parent is Wine's)
#   2. a Wine window brought forward covers it, and it comes forward again
#      (X stacking, as drawn: the pixel where they overlap)
#   3. a click into it activates its frame; typed keys reach it once its
#      frame is active, the pointer elsewhere
#   4. WM_CLOSE to the frame (its close button, End task) closes it, and the
#      frame goes
#
#   WINE=/opt/wine-sg/bin/wine test/linuxembed-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb xdotool xterm xwininfo import cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-linuxembed.XXXXXX); XP=; XT=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XT" ] && kill "$XT" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
xterm -geometry 50x12+120+120 -bg black -fg white -title 'Linux Terminal' -e sh -c "cd '$T'; exec sh" & XT=$!
i=0; while [ -z "$(xwininfo -root -tree | awk '/"Linux Terminal"/ {print $1; exit}')" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
XID=$(xwininfo -root -tree | awk '/"Linux Terminal"/ {print $1; exit}')
printf '%d shown - XTerm\tLinux Terminal\nEND\n' "$XID" > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
WINEDEBUG="${EMBED_DEBUG:--all}" "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 5    # the taskbar reads the list
cd "$WINEPREFIX/drive_c"

# 1.
parent=$(xwininfo -id "$XID" -tree 2>/dev/null | awk '/Parent window id:/ {print $5}')
pname=$(xwininfo -id "$XID" -tree 2>/dev/null | sed -n 's/.*Parent window id: [^ ]* //p')
case "$pname" in *"explorer.exe"*|*"Linux Terminal"*) [ "$pname" != "(the root window)" ] && pass "the Linux window is in a Wine window ($parent)" ;;
    *) fail "the Linux window's parent: $pname" ;; esac

# 2. Notepad over it, then it over Notepad: the pixel where they overlap
"$WINE" notepad > /dev/null 2>&1 &
sleep 4
"$WINE" probe.exe move Notepad "" 200 160 500 400 > /dev/null 2>&1
"$WINE" probe.exe activate Notepad "" > /dev/null 2>&1; sleep 1.5
px() { import -window root -crop 1x1+260+230 txt:- 2>/dev/null | tail -1 | grep -o '#[0-9A-F]\{6\}'; }
p1=$(px)
"$WINE" probe.exe activate SgLinuxWindow "Linux Terminal" > /dev/null 2>&1; sleep 1.5
p2=$(px)
[ "$p1" != "#000000" ] && [ "$p2" = "#000000" ] && pass "it stacks among Wine's windows: under Notepad ($p1), then over it ($p2)" \
    || fail "stacking: with Notepad active $p1, with it active $p2 (want not black, then black)"

# 2b. a click into it, Notepad active: its frame comes forward
"$WINE" probe.exe activate Notepad "" > /dev/null 2>&1; sleep 1
xdotool mousemove 150 150; sleep 0.3; xdotool click 1; sleep 1.5
"$WINE" probe.exe foreground SgLinuxWindow "Linux Terminal" | grep -q 'foreground=1' \
    && pass "a click into it brings its frame forward" || fail "a click into it did not activate its frame"

# 3. keys, the pointer away from it
xdotool mousemove 900 650; sleep 0.3
xdotool type --delay 40 'touch typed-here'; xdotool key Return; sleep 1.5
[ -e "$T/typed-here" ] && pass "keys reach it when its frame is active" || fail "typed keys did not reach it"

# 4. its close button (a click on the frame, not into the program)
"$WINE" probe.exe rect SgLinuxWindow "Linux Terminal" > "$T/rect" 2>/dev/null
set -- $(cat "$T/rect")
if [ $# = 4 ]; then
    xdotool mousemove $(( $3 - 18 )) $(( $2 + 14 )); sleep 0.5; xdotool click 1
    i=0; while kill -0 "$XT" 2>/dev/null && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
fi
kill -0 "$XT" 2>/dev/null && fail "its frame's close button did not close it" || { pass "its frame's close button closes it"; XT=; }
sleep 1
xterm -geometry 50x12+120+120 -bg black -fg white -title 'Linux Terminal' & XT=$!
i=0; while [ -z "$(xwininfo -root -children | awk '/"Linux Terminal"/ {print $1; exit}')" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
printf '%d shown - XTerm\tLinux Terminal\nEND\n' "$(xwininfo -root -children | awk '/"Linux Terminal"/ {print $1; exit}')" > "$T/list"
sleep 4   # framed at the next tick
"$WINE" probe.exe wmclose SgLinuxWindow "Linux Terminal" > /dev/null 2>&1
i=0; while kill -0 "$XT" 2>/dev/null && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
kill -0 "$XT" 2>/dev/null && fail "WM_CLOSE to its frame did not close it" || { pass "WM_CLOSE to its frame closes it"; XT=; }
printf 'END\n' > "$T/list"   # gone from the compositor's list too
i=0; while ! "$WINE" probe.exe exists SgLinuxWindow "" | grep -q 'exists=0' && [ $i -lt 24 ]; do sleep 0.5; i=$((i + 1)); done
"$WINE" probe.exe exists SgLinuxWindow "" | grep -q 'exists=0' && pass "...and the frame goes" || fail "the frame stayed"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
