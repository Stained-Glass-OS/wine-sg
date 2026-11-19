#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ncrypt PKCS #8 for EC keys (wine-sg 8003): NCryptExportKey(PKCS8_PRIVATEKEY) of a P-256/P-384 key
# encrypted under a secret, and NCryptImportKey of an EC PrivateKeyInfo, plain or encrypted (both were
# NTE_NOT_SUPPORTED): export, wrong secret refused, import back, the imported key verifies the original's
# signature; the export policy still holds. 64/32 bit. test/ncryptecp8-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/ncryptecp8-gate.sh      (ARCHS="x86_64" for one architecture)
# Mutant: SG_MUTANT_PKCS8_NO_EC (ncrypt pkcs8.c).
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
T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-ecp8.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in $ARCHS; do
    TMPDIR="$T" $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/ncryptecp8-probe.c" -lncrypt \
        || { echo "FAIL  probe did not build"; exit 1; }
done
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
