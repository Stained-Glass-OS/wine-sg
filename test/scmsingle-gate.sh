#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# One Service Control Manager (patches/sg/0530): a services.exe started
# while the SCM runs leaves at once with ERROR_SERVICE_ALREADY_RUNNING, as on
# Windows. Two of them served the same \pipe\svcctl, each with its own
# database: a service's status reaching the other one left its starter
# waiting until ERROR_SERVICE_REQUEST_TIMEOUT (1053), and Microsoft Office's
# Click-to-Run installer ended on its own service (ClickToRunSvc).
#
#   WINE=/opt/wine-sg/bin/wine test/scmsingle-gate.sh
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
T=$(mktemp -d /var/tmp/sg-scmsingle.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/scmsingle-probe.c" -ladvapi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1=//p" "$T/out"; }
[ "$(v before)" = 1 ] && pass "one services.exe runs" || fail "before: $(v before)"
[ "$(v second)" = "exited 1056" ] && pass "a second services.exe leaves at once: ERROR_SERVICE_ALREADY_RUNNING" \
    || fail "second services.exe: $(v second) (expected 'exited 1056')"
[ "$(v after)" = 1 ] && pass "one services.exe afterwards" || fail "after: $(v after)"
[ "$(v start)" = 1 ] && [ "$(v state)" = 4 ] && pass "the SCM starts a service (MSIServer running)" \
    || fail "service start: start=$(v start) state=$(v state)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
