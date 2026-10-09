#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# crypt32 async handles (CryptCreate/Set/Get/CloseAsync*) and the Unicode
# wrappers of the registry and CryptoAPI calls (Reg*U, CryptEnumProvidersU,
# CryptSetProviderU, CryptSignHashU, CryptVerifySignatureU), patches/sg/2408,
# 64- and 32-bit; the probe is test/crypt32aux-probe.c. All of them were stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/crypt32aux-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (crypt32/async.c): SG_MUTANT_ASYNC_INTRES, _NOFREE, _REPLACE, _HANDLE,
# _GET; and by hand in crypt32.spec: a Reg*U or Crypt*U entry forwarded to the
# wrong function.
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
T=$(mktemp -d /var/tmp/sg-crypt32aux.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/crypt32aux-probe.c" -luser32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
for a in x86_64 i686; do
    echo "== $a"
    # each run starts from a fresh profile: the probe leaves a registry key and handles behind
    "$WINESERVER" -k 2>/dev/null; rm -rf "$WINEPREFIX"; mkdir -p "$WINEPREFIX"
    timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
    "$WINESERVER" -w
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
