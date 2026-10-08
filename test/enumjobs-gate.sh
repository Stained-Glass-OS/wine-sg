#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# EnumJobs and the page calls of a raw print job (patches/sg/1615), 64- and
# 32-bit: test/enumjobs-probe.c, on a printer it makes with Wine's
# PostScript driver and the FILE: port. EnumJobsW failed with no error
# (779 FIXMEs in the logs); StartPagePrinter/EndPagePrinter did nothing.
#
#   WINE=/opt/wine-sg/bin/wine test/enumjobs-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_NO_ENUM_JOBS (localspl/provider.c).
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
T=$(mktemp -d /var/tmp/sg-enumjobs.XXXXXX)
export CUPS_SERVER="$T/no-cups.sock" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -municode -O1 -o "$T/p64.exe" "$HERE/enumjobs-probe.c" -lwinspool &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -municode -O1 -o "$T/p32.exe" "$HERE/enumjobs-probe.c" -lwinspool || { echo "FAIL  probe did not build"; exit 1; }
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
