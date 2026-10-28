#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A root's enhanced key usage property limits what its chains are good for (patches/sg/7602).
#
# Windows' root program marks each root with what it is trusted for: a code
# signing root (VeriSign Class 3 G5, DigiCert's code signing roots) only for
# Code Signing and Time Stamping. Wine's chain engine read only the
# certificates' own extensions, so such a root in the machine store would be
# trusted for a web server too. A test CA (openssl) goes into the machine's
# Root store marked Code Signing only; a leaf it issued for both uses is then
# valid for code signing and not for server authentication. Unmarked, both,
# as before. Mutant SG_MUTANT_NO_ROOT_EKU_PROP.
#
#   WINE=/opt/wine-sg/bin/wine test/rooteku-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v openssl >/dev/null || { echo "SKIP: no openssl"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-rooteku.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/rooteku-probe.c" -lcrypt32 || { echo "FAIL  probe did not build"; exit 1; }
cd "$T" || exit 1
# two test CAs, each with a leaf for both uses: one is marked, the other not
for c in marked plain; do
    openssl req -x509 -newkey rsa:2048 -nodes -keyout $c-ca.key -out $c-ca.pem -days 3650 -subj "/CN=SG Gate $c Root" \
        -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" >/dev/null 2>&1
    openssl req -newkey rsa:2048 -nodes -keyout $c-leaf.key -out $c-leaf.csr -subj "/CN=$c.sg-gate.example" >/dev/null 2>&1
    printf 'basicConstraints=CA:FALSE\nextendedKeyUsage=serverAuth,codeSigning\nsubjectAltName=DNS:%s.sg-gate.example\n' $c > $c-leaf.ext
    openssl x509 -req -in $c-leaf.csr -CA $c-ca.pem -CAkey $c-ca.key -CAcreateserial -out $c-leaf.pem -days 365 -extfile $c-leaf.ext >/dev/null 2>&1
    openssl x509 -in $c-ca.pem -outform DER -out $c-ca.der && openssl x509 -in $c-leaf.pem -outform DER -out $c-leaf.der \
        || { echo "FAIL  test certificates"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
fail=0
# CERT_TRUST_IS_NOT_VALID_FOR_USAGE 0x10; untrusted root 0x20
bits() { printf '%s\n' "$1" | sed -n "s/^$2 //p" | tr -d '\r'; }
out=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" 'Z:'"$T/plain-ca.der" 'Z:'"$T/plain-leaf.der" noprop 2>/dev/null | tr -d '\r')
echo "      unmarked: $(printf '%s' "$out" | tr '\n' ' ')"
[ "$(bits "$out" SERVER)" = 00000000 ] && [ "$(bits "$out" CODE)" = 00000000 ] \
    && echo "PASS  an unmarked root: its chains are good for server authentication and code signing, as before" \
    || { echo "FAIL  unmarked root: $out"; fail=1; }
out=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" 'Z:'"$T/marked-ca.der" 'Z:'"$T/marked-leaf.der" prop 2>/dev/null | tr -d '\r')
echo "      marked Code Signing: $(printf '%s' "$out" | tr '\n' ' ')"
[ "$(bits "$out" CODE)" = 00000000 ] && echo "PASS  a root marked Code Signing only: good for code signing" || { echo "FAIL  code signing: $out"; fail=1; }
case "$(bits "$out" SERVER)" in *1?) echo "PASS  ...and not for a web server (CERT_TRUST_IS_NOT_VALID_FOR_USAGE)" ;;
  *) echo "FAIL  a Code Signing root trusted for a web server: $out"; fail=1 ;; esac
[ $fail = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
