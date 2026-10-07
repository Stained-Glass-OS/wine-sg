#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wininet's INTERNET_OPTION_LISTEN_TIMEOUT (patches/sg/1473): Wine refused it
# (ERROR_INTERNET_INVALID_OPTION); Windows accepts it on any handle, and
# Office's HTTP client (OneAuth) sets it on its connection handle and treats
# a refusal as a failed request. The probe opens an HTTP connect handle (no
# traffic is sent) and sets the option, and the timeouts OneAuth sets next to it.
#
#   WINE=/opt/wine-sg/bin/wine test/wininet-listentimeout-gate.sh
# Mutant: SG_MUTANT_WININET_NO_LISTEN_TIMEOUT (wininet internet.c).
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
T=$(mktemp -d /var/tmp/sg-listentimeout.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/wininet-listentimeout-probe.c" -lwininet || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
[ "$(v connect)" = "1 0" ] && pass "an HTTP connect handle opens" || fail "connect: $(v connect)"
[ "$(v connect_timeout)" = "1 0" ] && pass "INTERNET_OPTION_CONNECT_TIMEOUT is accepted on it" || fail "connect_timeout: $(v connect_timeout)"
[ "$(v send_timeout)" = "1 0" ] && pass "INTERNET_OPTION_SEND_TIMEOUT is accepted on it" || fail "send_timeout: $(v send_timeout)"
[ "$(v receive_timeout)" = "1 0" ] && pass "INTERNET_OPTION_RECEIVE_TIMEOUT is accepted on it" || fail "receive_timeout: $(v receive_timeout)"
[ "$(v listen_timeout)" = "1 0" ] && pass "INTERNET_OPTION_LISTEN_TIMEOUT is accepted on it" || fail "listen_timeout: $(v listen_timeout)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
