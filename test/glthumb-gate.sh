#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Task View's and Alt+Tab's picture of a window that draws through
# Direct3D, OpenGL or Vulkan (wine-sg 0816). Such a window presents straight
# to its X window; what Wine keeps of it, which PrintWindow gives, is black:
# Sonos (WPF) showed as a black card in both (David 2026-10-05). The picture
# is now taken from the screen while it is in front, and an older one is
# not replaced by black. Under Xvfb, with the shell: a probe drawing green
# through OpenGL only.
#   1. Task View's card for it is green, not black
#   2. so is Alt+Tab's
#
#   WINE=/opt/wine-sg/bin/wine test/glthumb-gate.sh   (mutant SG_MUTANT_D3D_THUMB_BLACK)
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
T=$(mktemp -d /var/tmp/sg-glthumb.XXXXXX); XP=
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
"$WINE" "$T/probe.exe" >/dev/null 2>&1 &
sleep 6
green() { convert "$1" -fuzz 8% -fill black +opaque '#1ACC33' -fill white -opaque '#1ACC33' -format '%[fx:int(mean*w*h)]' info: 2>/dev/null; }
import -window root "$T/desk.png" 2>/dev/null
[ "$(green "$T/desk.png")" -gt 20000 ] 2>/dev/null || { echo "SKIP: no OpenGL drawing on this X server ($(green "$T/desk.png") green pixels)"; exit 77; }
# what the window covers, painted over: only a card can be green then
xdotool key super+Tab; sleep 3
import -window root "$T/taskview.png" 2>/dev/null
xdotool key Escape; sleep 1.5
n=$(green "$T/taskview.png")
[ "${n:-0}" -gt 2000 ] && pass "Task View's card for an OpenGL window shows it ($n green pixels)" \
    || fail "Task View: ${n:-no} green pixels (its card black)"
xdotool keydown alt; sleep 0.3; xdotool key Tab; sleep 1.5
import -window root "$T/alttab.png" 2>/dev/null
xdotool keyup alt; sleep 1
# the switcher's picture, in the middle of the screen (over the window itself)
convert "$T/alttab.png" -crop 160x100+432+344 +repage "$T/alttab-card.png"
n=$(green "$T/alttab-card.png")
[ "${n:-0}" -gt 2000 ] && pass "and Alt+Tab's ($n green pixels in its picture)" || fail "Alt+Tab: ${n:-no} green pixels (its card black)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
