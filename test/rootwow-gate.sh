#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# crypt32: a root an administrator adds to HKLM\Root stays when a 32-bit program starts (6027).
# HKLM\Root is shared by 32- and 64-bit programs, but crypt32's record of the roots it imported from
# the host (Software\Wine\Crypt32 ImportedRootCerts) was read in each program's own registry view: a
# 32-bit program found none, took every root for an imported one, and its host sync deleted the
# administrator's root -- an installer or a browser of the other bitness then failed every signature
# and certificate that chains to it.
#   WINE=/opt/wine-sg/bin/wine test/rootwow-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc openssl osslsigncode; do
    command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-rootwow.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"

x86_64-w64-mingw32-gcc -O2 -o "$T/trust64.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 &&
i686-w64-mingw32-gcc -O2 -o "$T/trust32.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"

# a root, a code-signing certificate under it, and a program signed with it
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/ca.key" -out "$T/ca.pem" -days 2 \
    -subj "/CN=Stained Glass Gate Root" -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout "$T/signer.key" -out "$T/signer.csr" -subj "/CN=Stained Glass Test Publisher" 2>/dev/null
printf 'basicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=codeSigning\n' > "$T/ext.cnf"
openssl x509 -req -in "$T/signer.csr" -CA "$T/ca.pem" -CAkey "$T/ca.key" -CAcreateserial \
    -out "$T/signer.pem" -days 2 -extfile "$T/ext.cnf" 2>/dev/null
openssl x509 -in "$T/ca.pem" -outform DER -out "$C/ca.der"
for a in 64 32; do
    osslsigncode sign -certs "$T/signer.pem" -key "$T/signer.key" -h sha256 -in "$T/trust$a.exe" -out "$C/trust$a.exe" \
        >/dev/null 2>&1 || { fail "osslsigncode could not sign"; exit 1; }
done

run() { (cd "$C" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}

out=$(run trust64.exe 'C:\trust64.exe')
expect "$out" WinVerifyTrust 0x800b010a "before the root is added the signature does not chain to a trusted root"
out=$(run trust64.exe --root 'C:\ca.der')
expect "$out" root added "a 64-bit program adds the root to HKLM\\Root"
out=$(run trust64.exe 'C:\trust64.exe')
expect "$out" WinVerifyTrust 0 "a 64-bit program trusts the signature"
"$WINESERVER" -w
out=$(run trust32.exe 'C:\trust32.exe')
expect "$out" WinVerifyTrust 0 "the first 32-bit program trusts it too"
out=$(run trust64.exe 'C:\trust64.exe')
expect "$out" WinVerifyTrust 0 "and the root is still there for 64-bit programs after it"
out=$(run trust32.exe 'C:\trust32.exe')
expect "$out" WinVerifyTrust 0 "and for the next 32-bit one"
"$WINESERVER" -k; "$WINESERVER" -w
out=$(run trust32.exe 'C:\trust32.exe')
expect "$out" WinVerifyTrust 0 "after the server restarts too"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
