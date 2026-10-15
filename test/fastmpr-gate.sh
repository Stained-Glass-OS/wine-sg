#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mpr gap-A batch (patch 3222), 64- and 32-bit: test/fastmpr-probe.c with the test network provider
# test/fastmpr-provider.c registered in the scratch prefix: MultinetGetConnectionPerformance[AW] through
# NPGetConnectionPerformance and the LPT port WNetUseConnection picks for a printer (nothing real is connected).
#
#   WINE=/opt/wine-sg/bin/wine test/fastmpr-gate.sh
# Mutants: SG_MUTANT_MPR_* (mpr/wnet.c).
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
T=$(mktemp -d /var/tmp/sg-fastmpr.XXXXXX)
export WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/fastmpr-probe.c" -lmpr -ladvapi32 \
        && TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -shared -o "$T/provider-$a.dll" "$HERE/fastmpr-provider.c" -Wl,--kill-at \
        || { echo "FAIL  probe did not build ($a)"; exit 1; }
done
for a in x86_64 i686; do
    echo "== $a"
    export WINEPREFIX="$T/prefix-$a"
    mkdir -p "$WINEPREFIX"
    timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
    "$WINESERVER" -w
    cp "$T/probe-$a.exe" "$WINEPREFIX/drive_c/fastmpr-probe.exe"
    cp "$T/provider-$a.dll" "$WINEPREFIX/drive_c/fastmpr-provider.dll"
    out=$(cd "$WINEPREFIX/drive_c" && FASTMPR_LOG='C:\fastmpr.log' timeout -s KILL 200 env DISPLAY= "$WINE" 'C:\fastmpr-probe.exe' 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
    "$WINESERVER" -k 2>/dev/null
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
