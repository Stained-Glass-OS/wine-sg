#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# crypt32: CryptImportPKCS8, CryptExportPKCS8, CryptExportPKCS8Ex as Microsoft documents them (patches/sg/9920;
# test/pkcs8-probe.c; all three were spec stubs, so a caller crashed). openssl makes an RSA PKCS #8 key; the probe
# imports it (resolve callback, default provider, encrypted with decrypt/encrypt callbacks), checks the modulus,
# exports it byte for byte, marks/imports a signature-only key by its key usage attribute, refuses DSA with
# ERROR_UNSUPPORTED_TYPE; openssl checks the export. Mutant: SG_MUTANT_PKCS8_RSA_UNSUPPORTED (dlls/crypt32/pfx.c).
#   WINE=/opt/wine-sg/bin/wine test/pkcs8-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v openssl >/dev/null || { echo "SKIP: needs openssl"; exit 77; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pkcs8.XXXXXX)
P=pkcs8-probe; LIBS="-lcrypt32 -ladvapi32"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/$P.exe" "$HERE/$P.c" $LIBS 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; echo "RESULT: FAIL"; exit 1; }
command -v i686-w64-mingw32-gcc >/dev/null &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/${P}32.exe" "$HERE/$P.c" $LIBS 2>>"$T/cc.log"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:1024 -out "$T/key.pem" 2>/dev/null &&
    openssl pkcs8 -topk8 -nocrypt -in "$T/key.pem" -outform DER -out "$T/key.der" 2>/dev/null &&
    openssl rsa -inform DER -in "$T/key.der" -noout -modulus 2>/dev/null | sed 's/^Modulus=//' > "$T/modulus.txt" ||
    { echo "FAIL  openssl did not make a key"; echo "RESULT: FAIL"; exit 1; }
rc=0
for p in $P ${P}32; do
    [ -f "$T/$p.exe" ] || continue
    echo "== $p"
    (cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" key.der modulus.txt "out-$p.der" </dev/null 2>/dev/null) | tr -d '\r' > "$T/$p.out"
    cat "$T/$p.out"
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
    if openssl pkey -inform DER -in "$T/out-$p.der" -noout 2>/dev/null &&
        [ "$(openssl rsa -inform DER -in "$T/out-$p.der" -noout -modulus 2>/dev/null | sed 's/^Modulus=//')" = "$(cat "$T/modulus.txt")" ]; then
        echo "PASS  openssl reads the export, with the same modulus"
    else echo "FAIL  openssl does not read the export as the same key"; rc=1; fi
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
