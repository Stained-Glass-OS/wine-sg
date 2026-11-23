#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# BCrypt ChaCha20-Poly1305: opened by name, its properties, encrypt/decrypt
# with a 12-byte nonce and a 16-byte tag against RFC 8439's 2.8.2 vector, a
# bad tag refused (patches/sg/9304), 64- and 32-bit: test/bcchacha-probe.c.
# Before, the algorithm was not found (.NET ChaCha20Poly1305.IsSupported false).
#
#   WINE=/opt/wine-sg/bin/wine test/bcchacha-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_NO_CHACHA20 (bcrypt/gnutls.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-bcchacha.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/bcchacha-probe.c" -lbcrypt &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/bcchacha-probe.c" -lbcrypt || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
