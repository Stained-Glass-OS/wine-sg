#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Type library writing and reading (patches/sg/1675), 64- and 32-bit:
# test/tlbcreate-probe.c writes a type library with CreateTypeLib2 (a help
# string context, a module function as a DLL entry, custom data on a
# function, a parameter, a variable and an implemented interface, deleted
# members and a deleted type info), reads it back, asks GetLibStatistics,
# invokes a constant and a record's variables, and registers it for the
# user only (RegisterTypeLibForUser, found by LoadRegTypeLib) and back.
# These were stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/tlbcreate-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_FIXED_STATS, SG_MUTANT_NO_MEMBER_CUSTDATA,
# SG_MUTANT_NO_VAR_INVOKE, SG_MUTANT_USER_IS_MACHINE (oleaut32/typelib.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-tlbcreate.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/tlbcreate-probe.c" -loleaut32 -lole32 -luuid -ladvapi32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
