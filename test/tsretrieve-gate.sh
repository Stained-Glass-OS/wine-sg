#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# RFC 3161 time stamps in crypt32 (wine-sg 7855): CryptRetrieveTimeStamp and
# CryptVerifyTimeStampSignature were missing (LibreOffice imports both: a document signature with a
# time stamp ended at "unimplemented function"). A local time-stamping authority -- a test CA and a
# TSA certificate (extendedKeyUsage timeStamping) made here, `openssl ts -reply` behind
# tsretrieve-server.py -- and test/tsretrieve-probe.c (64/32-bit): a time stamp retrieved with
# verification, nonce and certificates (imprint, nonce, time, policy, signer, store), the token
# verified again for its data and refused for other data or a changed signature, a hash given
# instead of the data, an authority's refusal; and OpenSSL verifies the token we retrieved.
#   WINE=/opt/wine-sg/bin/wine test/tsretrieve-gate.sh
#   (mutant SG_MUTANT_TS_NO_IMPRINT_CHECK, dlls/crypt32/timestamp.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in openssl python3 x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
T=$(mktemp -d /var/tmp/sg-tsretrieve.XXXXXX); SP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$SP" ] && kill "$SP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
unset DISPLAY WAYLAND_DISPLAY
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -municode -o "$T/probe-$a.exe" "$HERE/tsretrieve-probe.c" -lcrypt32 -lbcrypt \
        || { echo "FAIL  the probe ($a) did not build"; exit 1; }
done

# the authority: a CA, its TSA certificate, openssl ts's configuration
mkdir -p "$T/tsa"; cd "$T/tsa"
openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.pem -days 2 -subj "/CN=SG Test TSA CA" >/dev/null 2>&1
openssl req -newkey rsa:2048 -nodes -keyout tsa.key -out tsa.csr -subj "/CN=SG Test TSA" >/dev/null 2>&1
printf 'extendedKeyUsage=critical,timeStamping\nkeyUsage=critical,digitalSignature\n' > tsa.ext
openssl x509 -req -in tsa.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out tsa.pem -days 2 -extfile tsa.ext >/dev/null 2>&1
echo 01 > serial
cat > tsa.cnf <<CNF
[ tsa ]
default_tsa = tsa_config
[ tsa_config ]
dir = $T/tsa
serial = \$dir/serial
signer_cert = \$dir/tsa.pem
certs = \$dir/ca.pem
signer_key = \$dir/tsa.key
signer_digest = sha256
default_policy = 1.3.6.1.4.1.65535.7855.1
digests = sha1, sha256, sha384, sha512
accuracy = secs:1, millisecs:500
ordering = no
ess_cert_id_alg = sha256
CNF
[ -s tsa.pem ] || { echo "FAIL  the test authority's certificates were not made"; exit 1; }
python3 -I "$HERE/tsretrieve-server.py" "$T/tsa" "$T/port" 2>"$T/server.err" & SP=$!
i=0; while [ ! -s "$T/port" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
URL="http://127.0.0.1:$(cat "$T/port")/"
cd "$T"

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
RC=0
for a in x86_64 i686; do
    echo "== $a"
    mkdir -p "$T/out-$a"
    out=$(cd "$T/out-$a" && timeout -s KILL 180 "$WINE" "$T/probe-$a.exe" "$URL" . 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
    if [ -s "$T/out-$a/token.der" ]; then
        if openssl ts -verify -in "$T/out-$a/token.der" -token_in -data "$T/out-$a/data.bin" -CAfile "$T/tsa/ca.pem" \
               -untrusted "$T/tsa/tsa.pem" >/dev/null 2>&1; then
            echo "PASS  $a: OpenSSL verifies the token we retrieved"
        else echo "FAIL  $a: OpenSSL does not verify the token we retrieved"; RC=1; fi
    else echo "FAIL  $a: the probe wrote no token for OpenSSL to check ($(ls "$T/out-$a" | tr '\n' ' '))"; RC=1
    fi
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
