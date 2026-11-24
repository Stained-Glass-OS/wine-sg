#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wininet's per-connection option INTERNET_PER_CONN_FLAGS_UI (10) (patches/sg/9752). Firefox,
# Thunderbird and Pale Moon read the system proxy with it first; Wine had no such option ("Unknown
# dwOption 10", ERROR_INVALID_PARAMETER for the whole list) and they fell back to a second query.
# On Windows it is the connection's flags as the Internet Options dialog shows them. The probe
# (test/proxyflagsui-probe.c) checks:
#   FLAGS_UI and FLAGS in one list succeed and agree (the process's settings, NULL handle)
#   the same on a session handle
#   setting FLAGS_UI sets the flags (FLAGS reads back DIRECT|AUTO_DETECT)
#   WINE=/opt/wine-sg/bin/wine test/proxyflagsui-gate.sh
# Mutant (wininet/internet.c): SG_MUTANT_NO_FLAGS_UI.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-proxyflagsui.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/proxyflagsui-probe.c" -lwininet || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '      %s\n' "$out"
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
v() { printf '%s\n' "$out" | awk -v k="$1" -v f="$2" '$1 == k { print $f; exit }'; }
[ "$(v QUERY 2)" = ok ] && [ "$(v QUERY 3)" = "$(v QUERY 4)" ] && pass "FLAGS_UI and FLAGS in one list: both read, the same flags" || fail "query: '$(v QUERY 2) $(v QUERY 3) $(v QUERY 4)'"
[ "$(v QUERYSESSION 2)" = ok ] && [ "$(v QUERYSESSION 3)" = "$(v QUERYSESSION 4)" ] && pass "the same on a session handle" || fail "session: '$(v QUERYSESSION 2) $(v QUERYSESSION 3) $(v QUERYSESSION 4)'"
[ "$(v SET 2)" = ok ] && [ "$(v AFTER 2)" = 9 ] && pass "setting FLAGS_UI sets the connection's flags" || fail "set: '$(v SET 2) $(v SET 3)', flags after '$(v AFTER 2)' (want 9)"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
