#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Task Scheduler monthly triggers (patches/sg/5528), 64- and 32-bit:
# ScheduleByMonth (days of the month, "Last") and ScheduleByMonthDayOfWeek
# (weeks 1-4 and "Last", days of the week, months) are read by the schedule
# service from the task XML and listed by GetRunTimes (test/taskmonth-probe.c).
# They used to turn the trigger off.
#
#   WINE=/opt/wine-sg/bin/wine test/taskmonth-gate.sh
# Mutant (schedsvc/taskrun.c; x86_64_CFLAGS / i386_CFLAGS = "-g -O2 -DSG_MUTANT_MONTH_NO_LAST"):
# "Last" is ignored (days of the month and weeks).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-taskmonth.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/taskmonth-probe.c" -lole32 -loleaut32 -luuid &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/taskmonth-probe.c" -lole32 -loleaut32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
