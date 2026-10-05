#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Task View's and Alt+Tab's picture of a window that draws through
# Direct3D, OpenGL or Vulkan and whose class paints its background first
# (patches/sg/0843). 0816 took such a window's picture from the screen when
# what Wine kept of it was black; Chrome's is its background colour (dark
# gray) inside the frame Wine draws, so Chrome showed as an empty card in
# both. A picture whose client area is one colour is now "nothing drawn".
# Under Xvfb, with the shell: the probe drawing green through OpenGL over a
# dark gray class background.
#   1. Task View's card for it is green, not gray
#   2. so is Alt+Tab's
#
#   WINE=/opt/wine-sg/bin/wine test/glthumbgray-gate.sh   (mutant SG_MUTANT_THUMB_BLACK_ONLY)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb xdotool import convert "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-glthumbgray.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/glthumb-probe.c" -lopengl32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp +extension GLX 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run" SG_LOCK_CONTROL=/nonexistent
"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 &
sleep 8
"$WINE" "$T/probe.exe" gray >/dev/null 2>&1 &
sleep 6
green() { convert "$1" -fuzz 8% -fill black +opaque '#1ACC33' -fill white -opaque '#1ACC33' -format '%[fx:int(mean*w*h)]' info: 2>/dev/null; }
import -window root "$T/desk.png" 2>/dev/null
[ "$(green "$T/desk.png")" -gt 20000 ] 2>/dev/null || { echo "SKIP: no OpenGL drawing on this X server ($(green "$T/desk.png") green pixels)"; exit 77; }
xdotool key super+Tab; sleep 3
import -window root "$T/taskview.png" 2>/dev/null
xdotool key Escape; sleep 1.5
n=$(green "$T/taskview.png")
[ "${n:-0}" -gt 2000 ] && pass "Task View's card for an OpenGL window with a gray background shows it ($n green pixels)" \
    || fail "Task View: ${n:-no} green pixels (its card the background's gray)"
xdotool keydown alt; sleep 0.3; xdotool key Tab; sleep 1.5
import -window root "$T/alttab.png" 2>/dev/null
xdotool keyup alt; sleep 1
convert "$T/alttab.png" -crop 160x100+432+344 +repage "$T/alttab-card.png"
n=$(green "$T/alttab-card.png")
[ "${n:-0}" -gt 2000 ] && pass "and Alt+Tab's ($n green pixels in its picture)" || fail "Alt+Tab: ${n:-no} green pixels (its card gray)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
