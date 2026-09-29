#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The C++ library's time zone database (patches/sg/0504). Microsoft's STL
# asks msvcp140_atomic_wait for std::chrono's zones (__std_tzdb_*); Wine had
# stubs, and Word ended at start ("unimplemented function
# __std_tzdb_get_time_zones"). The answers come from the system's IANA
# database: the names from tzdata.zi (sorted, links pointing at their
# zone), a zone at an instant from its TZif file -- past the file's last
# transition from its POSIX rule -- and the current zone from /etc/timezone.
# Checked against the Linux C library's own answers (date(1)).
#
#   WINE=/opt/wine-sg/bin/wine test/tzdb-gate.sh
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
[ -r /usr/share/zoneinfo/tzdata.zi ] || { echo "SKIP: no tzdata"; exit 77; }
T=$(mktemp -d /var/tmp/sg-tzdb.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/tzdb-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
u() { date -u -d "$1" +%s; }
[ "$(v zones)" = "0 1 1 1 1" ] && pass "the zones and links, sorted; US/Eastern links to America/New_York" || fail "zones: $(v zones)"
[ "$(v version)" = 1 ] && [ "$(v current)" = "0 1" ] && pass "the database's version and the current zone" || fail "version/current: $(v version) / $(v current)"
[ "$(v berlin-summer)" = "7200 60 CEST $(u '2026-03-29 01:00') $(u '2026-10-25 01:00')" ] \
    && pass "Berlin in July 2026: CEST, +2h, 60 min saving, from the March change to the October one" || fail "berlin summer: $(v berlin-summer)"
[ "$(v berlin-winter)" = "3600 0 CET $(u '2025-10-26 01:00') $(u '2026-03-29 01:00')" ] \
    && pass "Berlin in January 2026: CET" || fail "berlin winter: $(v berlin-winter)"
[ "$(v newyork-2040)" = "-14400 60 EDT $(u '2040-03-11 07:00') $(u '2040-11-04 06:00')" ] \
    && pass "New York in 2040: EDT, from the file's POSIX rule" || fail "new york 2040: $(v newyork-2040)"
case "$(v utc)" in "0 0 UTC "*) pass "UTC: no offset, no saving" ;; *) fail "utc: $(v utc)" ;; esac
[ "$(v bad)" = "err 1" ] && pass "a name outside the database (../../etc/passwd) is refused" || fail "bad name: $(v bad)"
[ "$(v leap)" = "0 0" ] && pass "no leap seconds after those the library knows" || fail "leap: $(v leap)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
