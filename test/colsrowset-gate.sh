#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# OLE DB IColumnsRowset::GetColumnsRowset in MSDASQL (patches/sg/8221), 64-bit
# (the mock driver's SQLLEN is 64-bit), over test/adomock-driver.c:
# test/colsrowset-probe.c. It was E_NOTIMPL;
# ADO and OLE DB consumers ask it for the base table and column of each column.
#
#   WINE=/opt/wine-sg/bin/wine test/colsrowset-gate.sh
# Mutant: SG_MUTANT_NO_COLUMNS_ROWSET (msdasql/session.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for cc in x86_64-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-colsrowset.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for b in 64; do
    cc=x86_64-w64-mingw32-gcc
    TMPDIR=/var/tmp $cc -O2 -shared -o "$T/adomock$b.dll" "$HERE/adomock-driver.c" &&
    TMPDIR=/var/tmp $cc -O2 -o "$T/p$b.exe" "$HERE/colsrowset-probe.c" -lole32 -loleaut32 -luuid ||
        { fail "the probe or driver did not build ($b)"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T"/adomock64.dll "$T"/p64.exe "$C/"
"$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\SG ADO Mock' /v Driver /d 'C:\adomock64.dll' /f >/dev/null 2>&1
"$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\ODBC Drivers' /v 'SG ADO Mock' /d Installed /f >/dev/null 2>&1
printf '1\tAnn\t30\n2\tBob\t40\n' > "$C/people.tsv"
export SG_ADOMOCK_DATA='C:\people.tsv' SG_ADOMOCK_LOG='C:\adomock.log'
for b in p64; do
    out=$(cd "$C" && timeout -s KILL 120 "$WINE" "C:\\$b.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -q '^DONE' || { fail "$b: the probe did not finish"; continue; }
    for k in available columns_rowset columns_info row_values restart; do
        printf '%s\n' "$out" | grep -q "^$k=1" && pass "$b: $k" || fail "$b: $k ($(printf '%s\n' "$out" | grep "^$k=" || echo none))"
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
