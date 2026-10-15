#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE rpcrt4: pointer unmarshalling gaps: [allocate(all_nodes)] (one allocation, one free for a whole list) and the freeing of the
# data an object pointer had (direct calls of NdrPointerUnmarshall/NdrPointerFree with hand-made formats), 64+32 bit.
#
#   WINE=/path/to/build/wine test/rpcrt4-ptrgap-gate.sh   (WINESERVER=... when not beside $WINE)
# Mutants: PTRGAP_* (see patches/notes/3314.txt).
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
T=$(mktemp -d /var/tmp/sg-rpcrt4-ptrgap.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/rpcrt4-ptrgap-probe.c" -lrpcrt4 \
        || { echo "FAIL  probe did not build ($a)"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
ARCHES="x86_64 i686"; [ "${SG_QUICK:-}" = 1 ] && ARCHES=x86_64
for a in $ARCHES; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 100 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
