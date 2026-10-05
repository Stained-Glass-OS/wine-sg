#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# GetUserObjectInformation(UOI_IO) (patches/sg/0860): TRUE for the desktop
# that takes input, FALSE for another one, follows SwitchDesktop, and asks for
# a BOOL. AnyDesk asks it before it captures the screen; Wine answered
# ERROR_INVALID_PARAMETER and the connecting side saw "The remote desktop's
# image is inaccessible" over a black view.
#
#   WINE=/opt/wine-sg/bin/wine test/inputdesk-gate.sh
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

T=$(mktemp -d /var/tmp/sg-inputdesk.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/inputdesk-probe.c" -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }

[ "$(v own)" = "ret=1 io=1 needed=4 err=0" ] && pass "the thread's desktop takes input (TRUE, 4 bytes)" || fail "own: $(v own)"
[ "$(v other)" = "ret=1 io=0 needed=4 err=0" ] && pass "another desktop does not (FALSE)" || fail "other: $(v other)"
[ "$(v winsta)" = "ret=1 io=1 needed=4 err=0" ] && pass "the interactive window station does (TRUE)" || fail "winsta: $(v winsta)"
[ "$(v small)" = "ret=0 needed=4 err=122" ] && pass "a buffer smaller than a BOOL: ERROR_INSUFFICIENT_BUFFER, 4 needed" || fail "small buffer: $(v small)"
[ "$(v own-after-switch)" = "ret=1 io=0 needed=4 err=0" ] && [ "$(v other-after-switch)" = "ret=1 io=1 needed=4 err=0" ] \
    && pass "after SwitchDesktop the other desktop is the one" || fail "after switch: $(v own-after-switch) / $(v other-after-switch) $(v switch-failed)"
[ "$(v own-back)" = "ret=1 io=1 needed=4 err=0" ] && pass "and back" || fail "back: $(v own-back)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
