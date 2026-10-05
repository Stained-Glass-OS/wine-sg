#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# WinEvent hooks that skip their own process or thread (patches/sg/0861).
# "Own" is the process and thread that set the hook. A system-wide hook with
# WINEVENT_SKIPOWNPROCESS still got its own process's events: AnyDesk watches
# the foreground window that way and closed its own address box the moment
# the box took the focus -- only the first digit typed of an ID stayed.
# Out-of-context hooks, events from this thread, another thread of this
# process and another process.
#
#   WINE=/opt/wine-sg/bin/wine test/winevskip-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-winevskip.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/winevskip-probe.c" -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# windows without a display: Wine's null display driver
env DISPLAY= "$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }

[ "$(v hooks)" = "1 1 1" ] && pass "three hooks set" || fail "hooks: $(v hooks)"
[ "$(v own-thread)" = "1 0 0" ] && pass "an event from the hooking thread: skipped by both SKIPOWN hooks" || fail "own thread (all skipproc skipthread): $(v own-thread)"
[ "$(v other-thread)" = "1 0 1" ] && pass "from another thread of the process: skipped by SKIPOWNPROCESS only" || fail "other thread: $(v other-thread)"
[ "$(v other-process)" = "1 1 1" ] && pass "from another process: received by all three" || fail "other process: $(v other-process)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
