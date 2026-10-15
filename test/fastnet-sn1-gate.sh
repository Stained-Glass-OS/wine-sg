#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DECISIONS-2 NET-HOST (CORE review: security): StrongNameSignatureVerification[Ex] and StrongNameTokenFromAssembly[Ex]
# over assemblies signed by Wine Mono's mcs (test/net/sgmeta.cs, keys sgtest.snk / sgtest2.snk), 64/32 bit.
#
#   WINE=/opt/wine-sg/bin/wine test/fastnet-sn1-gate.sh      (ARCHS="x86_64" for one architecture)
# Mutants (mscoree/strongname.c): SG_MUTANT_SNV_ACCEPT, _ZERO, _SKIP, _CHECKSUM, _LASTSECT.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
ARCHS="${ARCHS:-x86_64 i686}"
for a in $ARCHS; do
    command -v $a-w64-mingw32-gcc >/dev/null || { echo "SKIP: $a-w64-mingw32-gcc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-netsn1.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in $ARCHS; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/fastnet-sn1-probe.c" -ladvapi32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
cp "$HERE"/net/sgmeta*.dll "$T/"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in $ARCHS; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
