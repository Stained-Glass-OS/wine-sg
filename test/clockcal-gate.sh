#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate: a click on the taskbar's clock opens its calendar (patches/sg/7603; David
# 2026-10-28: "the taskbar time/date does not come up with a calendar when you click
# on it"). Under Xvfb: the shell's taskbar, a stand-in for sg-shell's calendar
# (SgCalendarHost) counting the SgCalendarToggle messages, xdotool's pointer.
#   1. a click on the clock: the calendar is asked to open
#   2. a second click: asked again (it closes itself on that)
#   3. a click on the bar's empty middle: nothing
#   WINE=... test/clockcal-gate.sh
# Mutant: SG_MUTANT_CLOCK_NO_CALENDAR (programs/explorer/systray.c) fails 1.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
DPY=${DPY:-$((700 + $$ % 200))}
W=$(mktemp -d /var/tmp/clockcal-gate.XXXXXX)
fails=0 XP= EP= PP=
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
for t in Xvfb xdpyinfo xdotool "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
cleanup() { set +e; [ -n "$PP" ] && kill "$PP" 2>/dev/null; [ -n "$EP" ] && kill "$EP" 2>/dev/null; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP"; sleep 1; rm -rf "$W" "/tmp/.X${DPY}-lock"; }
trap cleanup EXIT
TMPDIR=/var/tmp "$MINGW" -O2 -o "$W/probe.exe" "$HERE/clockcal-probe.c" -luser32 || { fail "probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY" WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 & EP=$!
sleep 8
"$WINE" "$W/probe.exe" >/dev/null 2>&1 & PP=$!
C="$WINEPREFIX/drive_c/clockcal.count"
i=0; while [ ! -s "$C" ] && [ $i -lt 60 ]; do sleep 0.25; i=$((i + 1)); done
n() { tr -d '\r' < "$C" 2>/dev/null | sed -n 's/^toggles=//p'; }
wait_n() { i=0; while [ "$(n)" != "$1" ] && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done; }
# the clock: the bar's right end, at the bottom (40 px at 96 dpi)
xdotool mousemove 990 748; sleep 0.3; xdotool click 1; wait_n 1
[ "$(n)" = 1 ] && pass "a click on the clock asks the calendar to open" || fail "clock click: toggles=$(n)"
sleep 0.5; xdotool click 1; wait_n 2
[ "$(n)" = 2 ] && pass "a second click asks again (the calendar closes on it)" || fail "second click: toggles=$(n)"
xdotool mousemove 600 748; sleep 0.3; xdotool click 1; sleep 1.5
[ "$(n)" = 2 ] && pass "a click on the bar's empty middle does not" || fail "middle click: toggles=$(n)"
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$fails"
