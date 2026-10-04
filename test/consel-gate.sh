#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Selecting and copying in the console window (patches/sg/0786; David
# 2026-10-03: in PowerShell nothing could be highlighted, copied with a
# right-click or with Ctrl+Shift+C). As Windows 10's console: QuickEdit is on
# -- a mouse drag selects -- a right-click copies the selection (and ends it),
# or pastes when nothing is selected; Ctrl+Shift+C copies, Ctrl+Shift+V
# pastes. cmd in Wine's console window, driven with xdotool; the clipboard
# read and set with consel-clip.c.
#
#   WINE=/opt/wine-sg/bin/wine test/consel-gate.sh   (mutant SG_MUTANT_NO_QUICK_EDIT)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb xdotool "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-consel.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/clip.exe" "$HERE/consel-clip.c" || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp "$T/clip.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
clip() { "$WINE" 'C:\clip.exe' "$@" 2>/dev/null | tr -d '\r'; }
"$WINE" start /wait cmd /k "prompt $ " >/dev/null 2>&1 &
i=0; W=; while [ -z "$W" ] && [ $i -lt 40 ]; do sleep 0.5; W=$(xdotool search --name 'cmd.exe' 2>/dev/null | head -1); i=$((i + 1)); done
[ -n "$W" ] || { fail "no console window"; echo "RESULT: FAIL"; exit 1; }
xdotool windowactivate --sync "$W" 2>/dev/null; sleep 1
xdotool type --delay 40 'echo SGMARK-4711'; xdotool key Return; sleep 1.5
eval "$(xdotool getwindowgeometry --shell "$W")"
# drag over the top of the window, where the marker is, then a right-click
clip set nothing-yet
xdotool mousemove $((X + 4)) $((Y + 4)) mousedown 1; sleep 0.2
xdotool mousemove $((X + WIDTH - 20)) $((Y + 90)); sleep 0.2; xdotool mouseup 1; sleep 0.5
xdotool click 3; sleep 1
c=$(clip get)
case "$c" in *SGMARK-4711*) pass "a drag selects and a right-click copies it (the clipboard has the marker)" ;;
    *) fail "after drag + right-click the clipboard is: '$(echo "$c" | head -2 | tr '\n' '|')'" ;; esac
# Ctrl+Shift+C
clip set nothing-yet
xdotool mousemove $((X + 4)) $((Y + 4)) mousedown 1; sleep 0.2
xdotool mousemove $((X + WIDTH - 20)) $((Y + 90)); sleep 0.2; xdotool mouseup 1; sleep 0.5
xdotool key ctrl+shift+c; sleep 1
case "$(clip get)" in *SGMARK-4711*) pass "Ctrl+Shift+C copies the selection" ;; *) fail "Ctrl+Shift+C did not copy" ;; esac
# a right-click with nothing selected pastes
clip set "echo PASTED-9921"
xdotool click 3; sleep 0.5; xdotool key Return; sleep 1.5
xdotool mousemove $((X + 4)) $((Y + 4)) mousedown 1; sleep 0.2
xdotool mousemove $((X + WIDTH - 20)) $((Y + HEIGHT - 10)); sleep 0.2; xdotool mouseup 1; sleep 0.5
xdotool key ctrl+shift+c; sleep 1
n=$(clip get | grep -c 'PASTED-9921')
[ "${n:-0}" -ge 2 ] && pass "a right-click with nothing selected pastes (the pasted command ran)" || fail "paste: the marker appears $n times"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
