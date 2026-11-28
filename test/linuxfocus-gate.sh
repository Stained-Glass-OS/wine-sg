#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A framed Linux program in front keeps the keyboard when the session's window manager gives the X
# focus to the desktop window itself (patches/sg/9516). sg-compositor's X window manager activates
# the desktop's surface again when a Linux program's own top-level goes into its frame, or the
# Start menu closes, and sets the X focus on the desktop window: the program's frame stayed in front,
# its keys went to the desktop and nowhere -- a Linux app opened from its pinned taskbar icon took no
# typing until clicked (linux1's QA VM, 2026-11-27). A real xterm framed by the shell, in front:
#   - the X focus put on the desktop window (as the window manager does): it goes back into the
#     program;
#   - the person clicks the desktop itself: the focus stays there (not taken back).
#
#   WINE=/opt/wine-sg/bin/wine test/linuxfocus-gate.sh
#   mutant: SG_MUTANT_DESKTOP_TAKES_LINUX_FOCUS (dlls/winex11.drv/event.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
unset SESSION_MANAGER
for t in Xvfb xterm xwininfo xdotool cc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-linuxfocus.XXXXXX); XP=; XT=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; [ -n "$XT" ] && kill "$XT" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
printf 'END\n' > "$T/list"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 3
DESK=$(xwininfo -root -children | awk '/"shell - Wine Desktop"/ { print $1; exit }')
xterm -geometry 50x12+120+120 -title 'Linux Terminal' & XT=$!
i=0; while [ -z "$(xwininfo -root -tree | awk '/"Linux Terminal"/ {print $1; exit}')" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
XID=$(xwininfo -root -tree | awk '/"Linux Terminal"/ {print $1; exit}')
printf '%d shown - XTerm\tLinux Terminal\nEND\n' "$XID" > "$T/list"
sleep 6
focus() { xdotool getwindowfocus 2>/dev/null; }
[ "$(focus)" = "$(printf %d "$XID")" ] && pass "framed and in front, the program has the X focus" || fail "after framing, the focus is on $(focus) (program $(printf %d "$XID"))"
# the window manager's activation of the desktop's surface
xdotool windowfocus --sync "$DESK" 2>/dev/null || xdotool windowfocus "$DESK"; sleep 1.5
[ "$(focus)" = "$(printf %d "$XID")" ] && pass "the X focus put on the desktop window goes back into the program in front" || fail "the X focus stayed on $(focus) (desktop $(printf %d "$DESK"), program $(printf %d "$XID"))"
# the person clicks the desktop itself: the desktop is in front, the focus is its
xdotool mousemove 900 400 click 1; sleep 1.5
[ "$(focus)" != "$(printf %d "$XID")" ] && pass "the desktop clicked, the focus is not taken back by the program" || fail "the program took the focus back from the desktop the person clicked"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
