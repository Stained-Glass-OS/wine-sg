#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CERT_SHA256_HASH_PROP_ID (wine-sg 7783): crypt32 computes a certificate's SHA-256
# thumbprint on demand, as it does the SHA-1 one. Paint.NET 5.2's SetupFrontEnd checks its
# signer by it after WinVerifyTrust; the property was missing (CRYPT_E_NOT_FOUND), and setup
# said "its files have been modified or are damaged". test/certsha256-probe.c reads a fresh
# self-signed certificate and compares with openssl's fingerprint.
#   WINE=/opt/wine-sg/bin/wine test/certsha256-gate.sh
# Before 7783 it fails at SizeQuery (0x80092004).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc not installed"; exit 77; }
command -v openssl >/dev/null || { echo "SKIP: openssl not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-certsha256.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/certsha256-probe.c" -lcrypt32 \
    || { echo "FAIL  probe did not build"; exit 1; }
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/key.pem" -subj "/CN=SG certsha256 gate" \
    -days 2 -outform DER -out "$T/cert.der" >/dev/null 2>&1 || { echo "FAIL  no test certificate"; exit 1; }
want256=$(openssl x509 -inform DER -in "$T/cert.der" -noout -fingerprint -sha256 | sed 's/.*=//; s/://g' | tr A-F a-f)
want1=$(openssl x509 -inform DER -in "$T/cert.der" -noout -fingerprint -sha1 | sed 's/.*=//; s/://g' | tr A-F a-f)
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/probe.exe" cert.der 2>/dev/null </dev/null | tr -d '\r')
expect() {
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got: $(printf '%s' "$out" | tr '\n' ' ')"; fi
}
expect Created 1 "the certificate loads"
expect SizeQuery "1 32 0x00000000" "the SHA-256 property is there, 32 bytes"
expect Short "0 32 234" "a short buffer gives ERROR_MORE_DATA and the size"
expect Get "1 32" "it is read"
expect Sha256 "$want256" "it is the certificate's SHA-256 thumbprint"
expect Sha1 "$want1" "the SHA-1 thumbprint is unchanged"
expect Listed 1 "the computed value is kept on the context"
expect Set 1 "the property can be set"
expect AfterSet 1 "and a set value is what is read back"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
