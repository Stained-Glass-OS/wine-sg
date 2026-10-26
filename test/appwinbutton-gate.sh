#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An owned window with WS_EX_APPWINDOW has a taskbar button (7008), as on Windows ("Forces a
# top-level window onto the taskbar when the window is visible"). Delphi and Lazarus programs' main
# forms are owned by a hidden application window and ask for a button that way: the Lazarus IDE had
# none. An owned window without the style still has none, and an ordinary window still has one.
#   WINE=/opt/wine-sg/bin/wine test/appwinbutton-gate.sh   (SG_XDISPLAY=N to use Xvfb :N)
# Mutant: SG_MUTANT_OWNED_APPWINDOW (explorer systray.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in Xvfb $MINGW; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
T=$(mktemp -d /var/tmp/sg-appwinbutton.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
unset DISPLAY XAUTHORITY
XP=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/appwinbutton-probe.c" 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
if [ -n "${SG_XDISPLAY:-}" ]; then
    Xvfb ":$SG_XDISPLAY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$SG_XDISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
sleep 8
"$WINE" explorer /desktop=shell,1024x700 'C:\probe.exe' 'C:\probe.out' >/dev/null 2>&1 &
i=0; while ! grep -q done=1 "$WINEPREFIX/drive_c/probe.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
out=$(tr -d '\r' < "$WINEPREFIX/drive_c/probe.out" 2>/dev/null)
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^done=1$' || { fail "the probe did not finish"; echo "RESULT: FAIL"; exit 1; }
printf '%s\n' "$out" | grep -q '^owned=1$' || fail "the windows are not owned by a hidden window"
printf '%s\n' "$out" | grep -q '^button=owned-app$' && pass "the owned window with WS_EX_APPWINDOW has a button" \
    || fail "the owned window with WS_EX_APPWINDOW has no button"
printf '%s\n' "$out" | grep -q '^button=owned$' && fail "the owned window without WS_EX_APPWINDOW has a button" \
    || pass "the owned window without WS_EX_APPWINDOW has none"
printf '%s\n' "$out" | grep -q '^button=top$' && pass "an ordinary top-level window has its button" || fail "the top-level window's button is missing"
printf '%s\n' "$out" | grep -q '^button=owner$' && fail "the hidden owner has a button" || pass "the hidden owner has none"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
