#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The window that becomes active when the active one is hidden or destroyed
# (patches/sg/0862) is never one with WS_EX_NOACTIVATE -- the taskbar. Wine
# took the next window in Z order: when AnyDesk's address dropdown (topmost)
# closed, the taskbar (topmost, next) became active and took the keys typed
# into AnyDesk.
#
#   WINE=/opt/wine-sg/bin/wine test/noactnext-gate.sh
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

T=$(mktemp -d /var/tmp/sg-noactnext.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/noactnext-probe.c" -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# windows without a display: Wine's null display driver
env DISPLAY= "$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }

[ "$(v start)" = main ] && [ "$(v popup-active)" = 1 ] && pass "set up: the program's window active, then its dropdown" || fail "setup: $(v start) $(v popup-active)"
[ "$(v after-hide)" = main ] && pass "dropdown hidden: the program's window is active again, not the bar" || fail "after hide: $(v after-hide)"
[ "$(v focus-after-hide)" = main ] && pass "and has the keyboard" || fail "focus after hide: $(v focus-after-hide)"
[ "$(v after-destroy)" = main ] && pass "dropdown destroyed: the same" || fail "after destroy: $(v after-destroy)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
