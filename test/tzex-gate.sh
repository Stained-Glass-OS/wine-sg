#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The dynamic time zone conversions (patches/sg/0172):
# SystemTimeToTzSpecificLocalTimeEx and TzSpecificLocalTimeToSystemTimeEx
# were stubs (and SetDynamicTimeZoneInformation not exported, so a program
# importing it did not start); GetTimeZoneInformationForYear used today's
# rules for a year before a zone's Dynamic DST table.
#
#   WINE=/opt/wine-sg/bin/wine test/tzex-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-tzex.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/tzex-probe.exe" "$HERE/tzex-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(TZ=UTC timeout -s KILL 60 "$WINE" "$T/tzex-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() { if printf '%s\n' "$out" | grep -qx "$1"; then pass "$2"; else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" || echo none))"; fi; }
expect "exports=1,1,1" "kernel32 exports the three functions"
expect "pst_2006_march=2006-03-20 04:00" "20 March 2006 in Los Angeles: 2006's rules (standard time)"
expect "pst_2008_march=2008-03-20 05:00" "20 March 2008: the rules since 2007 (daylight time)"
expect "pst_2006_july=2006-07-01 05:00" "July 2006: daylight time"
expect "pst_1995_march=1995-03-20 04:00" "before the zone's table: its first year's rules"
expect "pst_2030_march=2030-03-20 05:00" "after it: its last year's rules"
expect "pst_2006_back=2006-03-20 12:00" "and back to UTC with 2006's rules"
expect "pst_2008_back=2008-03-20 12:00" "and with 2008's"
expect "pst_2006_disabled=2006-03-20 05:00" "dynamic rules disabled: the zone's standing rules"
expect "custom=2006-03-20 13:00" "a zone without a key name: the rules it carries"
expect "null_is_current=1" "no zone: the current one"
expect "fixed_2006_march=2006-03-20 05:00" "SystemTimeToTzSpecificLocalTime keeps one set of rules"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
