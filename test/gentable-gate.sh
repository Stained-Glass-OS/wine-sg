#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The non-AVL generic tables (patches/sg/2200), 64- and 32-bit (through
# wow64): test/gentable-probe.c builds an RTL_GENERIC_TABLE of ints through
# ntdll, checking insert, lookup, delete, sorted ("with splaying")
# enumeration, insertion-order enumeration and by-index access.
# RtlInsertElementGenericTable, RtlDeleteElementGenericTable and
# RtlIsGenericTableEmpty were "@ stub" (STATUS_NOT_IMPLEMENTED/crash from the
# PE side); RtlLookupElementGenericTable, RtlEnumerateGenericTable and
# RtlEnumerateGenericTableWithoutSplaying returned NULL unconditionally; a
# table never held anything.
#
#   WINE=/opt/wine-sg/bin/wine test/gentable-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (ntdll/rtl.c): SG_MUTANT_NO_INSERT_ORDER, SG_MUTANT_BAD_DELETE.
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
T=$(mktemp -d /var/tmp/sg-gentable.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/gentable-probe.c" \
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
