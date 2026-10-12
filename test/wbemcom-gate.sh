#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wbemprox COM semantics (patches/sg/2459), 64- and 32-bit (test/wbemcom-probe.c).
#
#   WINE=/opt/wine-sg/bin/wine test/wbemcom-gate.sh
# Mutants (wbemprox): SG_MUTANT_WBEM_CLIENTSEC_COUNT (services.c), SG_MUTANT_WBEM_CTX_ENUM (services.c), SG_MUTANT_WBEM_DERIVATION (query.c), SG_MUTANT_WBEM_WQL_ESCAPE (wql.y), SG_MUTANT_WBEM_QUAL_KEY (qualifier.c).
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
T=$(mktemp -d /var/tmp/sg-wbemcom.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/wbemcom-probe.c" -lole32 -loleaut32 -luuid -lwbemuuid \
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
