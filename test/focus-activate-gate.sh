#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Typing reaches a window activated from another process (patches/sg/0527).
# David: type in one program, switch to another with the taskbar, and
# "sometimes the input does not work" (Firefox, Notepad). The activated
# program asked X for the keyboard with a timestamp taken from its own last
# message -- older than the taskbar click that took the focus -- and the X
# server ignored the request: Wine said the window had the focus, X kept the
# keyboard elsewhere.
#
# Bravo is typed in, then Alpha is clicked and typed in; another process activates
# Bravo as the taskbar does; a key typed at the X level must reach Bravo.
#
#   WINE=/opt/wine-sg/bin/wine test/focus-activate-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb xdotool "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-focusact.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=${WINEDEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/focus-probe.exe" "$HERE/focus-probe.c" || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/focus-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 3
cd "$WINEPREFIX/drive_c"
"$WINE" focus-probe.exe window Bravo 560 320 'C:\bravo.log' >/dev/null 2>&1 & BP=$!
sleep 3
# typed in first, as a program one switches back to has been: its process now
# turns X times into its own (without that it would ask with CurrentTime)
xdotool mousemove 720 430 click 1; sleep 1
xdotool type --delay 50 x; sleep 1
"$WINE" focus-probe.exe window Alpha 80 80 'C:\alpha.log' >/dev/null 2>&1 &
sleep 3
xdotool mousemove 240 200 click 1; sleep 1   # into Alpha: X gives it the keyboard now
xdotool type --delay 50 a; sleep 1
# The taskbar's race, made certain. Bravo is held while another process
# activates it (as the taskbar does) and X focus then moves -- as the taskbar
# click moves it to the desktop -- at a server time later than the activation
# message Bravo is about to read. Released, Bravo asks X for the keyboard: with
# that message's time the server ignores it (older than the last focus change);
# with CurrentTime it is granted. On Xwayland the click's time comes from the
# compositor's clock and is later without any holding.
kill -STOP "$BP"
"$WINE" focus-probe.exe activate Bravo > "$T/act.out" 2>/dev/null
sleep 0.5
xdotool windowfocus --sync "$(xdotool search --name '^Alpha$' | head -1)" 2>/dev/null
sleep 0.3
kill -CONT "$BP"
act=$(tr -d '\r' < "$T/act.out")
sleep 1.5
xdotool type --delay 50 b; sleep 1.5
A=$(cat "$WINEPREFIX/drive_c/alpha.log" 2>/dev/null); B=$(cat "$WINEPREFIX/drive_c/bravo.log" 2>/dev/null)
echo "      alpha got '$A', bravo got '$B' ($act)"
[ "$A" = a ] && pass "a click and a key reach the clicked window" || fail "Alpha: '$A'"
[ "$B" = xb ] && pass "a window activated from another process (the taskbar) gets the keyboard" \
    || fail "typing after activation from another process went elsewhere (Bravo got '$B', Alpha '$A')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
