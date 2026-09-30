#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0531: in a virtual desktop, Wine's Z order is the X
# stacking. A window below others in Z order (a normal one under a topmost
# one) started at the top of the desktop's X children and stayed there: Word's
# splash covered its "unable to start" box, which was topmost. Under Xvfb, the
# shell desktop; the screen is read from X (import) between the probe's
# phases: a topmost window stays over a normal one shown later; a window
# raised to the top shows on top; a window put after another shows below it.
#   WINE=... test/xstack-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
DPY=${DPY:-$((700 + $$ % 200))}
W=$(mktemp -d /var/tmp/xstack-gate.XXXXXX)
fails=0 XP= EP= PP=
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
for t in Xvfb xdpyinfo import convert "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
cleanup() { set +e; [ -n "$PP" ] && kill "$PP" 2>/dev/null; [ -n "$EP" ] && kill "$EP" 2>/dev/null; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP"; sleep 1; rm -rf "$W" "/tmp/.X${DPY}-lock"; }
trap cleanup EXIT
TMPDIR=/var/tmp "$MINGW" -municode -O2 -o "$W/probe.exe" "$HERE/xstack-probe.c" -luser32 -lgdi32 || { fail "probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY" WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 & EP=$!
sleep 8
"$WINE" "$W/probe.exe" > "$W/out" 2>/dev/null & PP=$!
px() { import -window root "$W/s.png" 2>/dev/null; convert "$W/s.png" -format "%[pixel:p{$1,$2}]" info: 2>/dev/null; }
wait_phase() { local i=0; while ! grep -q "^phase=$1" "$W/out" 2>/dev/null && [ $i -lt 240 ]; do sleep 0.25; i=$((i + 1)); done; sleep 1.5; }
wait_phase 1
c1=$(px 200 175) c2=$(px 60 60)
wait_phase 2
c3=$(px 600 200)
wait_phase 3
c4=$(px 600 200)
wait "$PP" 2>/dev/null; PP=
tr -d '\r' < "$W/out" | sed 's/^/      /'
echo "      screen: topmost-over-later=$c1 later-alone=$c2 raised=$c3 put-after=$c4"
case "$c1" in *"0,0,255"*) pass "a topmost window stays over a normal one shown later (Word's box over its splash)";; *) fail "a normal window shown later covers the topmost one: $c1 (blue expected)";; esac
case "$c2" in *"255,0,0"*) pass "the later window shows where nothing covers it";; *) fail "later window not shown: $c2";; esac
case "$c3" in *"0,255,0"*) pass "a window raised to the top shows on top";; *) fail "raised window not on top: $c3 (green expected)";; esac
case "$c4" in *"255,255,0"*) pass "a window put after another shows below it (SetWindowPos insert-after)";; *) fail "window put after another still shows over it: $c4 (yellow expected)";; esac
echo "xstack-gate: $fails failure(s)"
[ "$fails" = 0 ]
