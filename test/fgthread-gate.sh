#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A hidden window of another thread does not take the foreground when it is
# positioned without SWP_NOACTIVATE (patches/sg/0461).
#
# Firefox's full-screen transition (the fade to black) runs on a thread of
# its own: it makes a hidden window and puts it on top with SetWindowPos,
# asking no SWP_NOACTIVATE. Wine made it the foreground window; the browser,
# deactivated, left full screen at once -- a video's full-screen button did
# nothing but flicker (field report 2). On Windows that activation is the
# thread's own, as SetActiveWindow. The foreground thread's own hidden window
# is still activated, as Wine's user32 tests have it.
#
#   WINE=/opt/wine-sg/bin/wine test/fgthread-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-fgthread.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/fgthread-probe.exe" "$HERE/fgthread-probe.c" -lgdi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 xvfb-run -a -s '-screen 0 1024x768x24' "$WINE" "$T/fgthread-probe.exe" 2>/dev/null | tr -d '\r')
echo "$out" | sed 's/^/      /'
echo "$out" | grep -qx "start 1" || fail "the probe's window never had the foreground"
echo "$out" | grep -q "other thread: foreground kept 1, deactivated 0" \
    && pass "another thread's hidden window leaves the foreground where it is (Firefox stays full screen)" \
    || fail "another thread's hidden window took the foreground"
echo "$out" | grep -q "its own active 1" && pass "and is that thread's own active window" || fail "not its thread's active window"
echo "$out" | grep -qx "same thread: hidden window active 1" \
    && pass "the foreground thread's own hidden window is activated (Windows, user32 tests)" || fail "same-thread activation changed"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
