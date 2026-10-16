#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# 5907 (CORE review: security): NCryptExportKey PKCS8_PRIVATEKEY under the key's export policy: in the
# clear only with NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG, encrypted under a secret with
# NCRYPT_ALLOW_EXPORT_FLAG alone; no export flag refuses both (NTE_NOT_SUPPORTED). 64/32 bit.
#   WINE=/opt/wine-sg/bin/wine test/ncrypt-p8policy-gate.sh      (ARCHS="x86_64" for one architecture)
# Mutants (ncrypt main.c): SG_MUTANT_P8_ENC_PLAINPOLICY (encrypted needs plaintext again),
# SG_MUTANT_EXPORT_ANYTHING (no policy): both fail this gate.
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
T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-p8pol.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in $ARCHS; do
    TMPDIR="$T" $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/ncrypt-p8policy-probe.c" -lncrypt \
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
