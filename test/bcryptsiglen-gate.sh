#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# BCryptGetProperty(key, BCRYPT_SIGNATURE_LENGTH) of a signing key, as Windows gives it (wine-sg
# 8405): the size of its signatures. Wine answered STATUS_NOT_SUPPORTED for every asymmetric key,
# and Bitvise SSH Client, which sizes the host key signature by it, ended every connection in key
# exchange ("AsymKey::CngSigLen: BCryptGetProperty: NT status 0xc00000bb").
# test/bcryptsiglen-probe.c, 64- and 32-bit: RSA 2048/1024 and ECDSA P-256/P-384 keys -- the
# property equals what BCryptSignHash writes; a symmetric key has none.
#   WINE=/opt/wine-sg/bin/wine test/bcryptsiglen-gate.sh
# Mutant: -DSG_MUTANT_NO_SIGNATURE_LENGTH (dlls/bcrypt/bcrypt_main.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-bcryptsiglen.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/bcryptsiglen-probe.c" -lbcrypt || { fail "$a probe did not build"; exit 1; }
done
unset DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for a in x86_64 i686; do
    cp "$T/probe-$a.exe" "$WINEPREFIX/drive_c/"
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "C:\\probe-$a.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $a: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "^$1=1"; then pass "$a: $2"
        else fail "$a: $2 ($(printf '%s\n' "$out" | grep "^$1=" | head -1 || echo none))"; fi
    }
    check rsa2048  "an RSA 2048 key's SignatureLength is 256, the size BCryptSignHash gives; a short buffer is STATUS_BUFFER_TOO_SMALL"
    check rsa1024  "RSA 1024: 128"
    check ecdsa256 "ECDSA P-256: 64"
    check ecdsa384 "ECDSA P-384: 96"
    check aes      "a symmetric key has none (STATUS_NOT_SUPPORTED)"
    check done        "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
