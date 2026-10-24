#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# The taskbar's Task View button is painted where it moves to (patches/sg/6529): when a
# notification-area icon goes away the button moves right with the desktop pager; it showed a
# cut-off fragment until hovered. Under Xvfb: the shell's taskbar, and a probe that adds and
# removes a tray icon, records the button's next WM_PAINT through a message hook, and compares the
# screen with a full repaint (test/taskviewmove-probe.c, test/taskviewmove-hook.c).
#   WINE=... test/taskviewmove-gate.sh       (DPY=N: that display)
# Mutant: SG_MUTANT_TASKVIEW_NO_REPAINT (programs/explorer/systray.c).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
DPY=${DPY:-$((700 + $$ % 200))}
W=$(mktemp -d /var/tmp/taskviewmove-gate.XXXXXX)
fails=0 XP= EP=
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
for t in Xvfb xdpyinfo "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
cleanup() { set +e; [ -n "$EP" ] && kill "$EP" 2>/dev/null; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP"; sleep 1; rm -rf "$W" "/tmp/.X${DPY}-lock"; }
trap cleanup EXIT
TMPDIR=/var/tmp "$MINGW" -O2 -o "$W/probe.exe" "$HERE/taskviewmove-probe.c" -luser32 -lgdi32 -lshell32 || { fail "probe did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O2 -shared -o "$W/taskviewmove-hook.dll" "$HERE/taskviewmove-hook.c" -luser32 || { fail "hook did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY" WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 & EP=$!
sleep 8
out=$(cd "$W" && timeout 60 "$WINE" probe.exe 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if grep -Eq "^$1=1( |$)" <<<"$out"; then pass "$2"
    else fail "$2 ($(grep "^$1=" <<<"$out" || echo none))"; fi
}
check found     "the taskbar's Task View button, left of the desktop pager"
check hooked    "a message hook in the taskbar's thread (test/taskviewmove-hook.c)"
check moved     "a tray icon moves it left, and its removal back"
check painted_whole "moved back, it is painted whole (its WM_PAINT's update rectangle is its client area)"
check repainted "where it moved to, the screen is what a full repaint gives (no fragment)"
check done      "the probe ran to the end"
echo "taskviewmove-gate: $fails failure(s)"
[ "$fails" = 0 ]
