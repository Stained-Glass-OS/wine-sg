#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# bcrypt key derivation providers (6088): BCryptOpenAlgorithmProvider("PBKDF2"/"HKDF"/"SP800_56A_CONCAT") was
# "not supported" (LibreOffice's xmlsec CNG back end asks for them) and BCryptKeyDerivation did not exist.
# Now: BCryptGenerateSymmetricKey on a KDF provider makes a key of the secret; BCryptKeyDerivation runs PBKDF2
# (RFC 8018), HKDF (RFC 5869: HkdfHashAlgorithm, HkdfSaltAndFinalize / HkdfPrkAndFinalize, KDF_HKDF_INFO), the
# SP 800-56A concatenation KDF and the SP 800-108 counter-mode HMAC KDF; BCryptDeriveKey(secret,
# "SP800_56A_CONCAT") runs the concatenation KDF over an ECDH agreement. Expected values: RFC vectors and
# Python's hashlib/hmac on the same inputs. 6089: the generic "ECDH" provider (curve by ECCCurveName, or by the
# key size; blobs with the generic magic) agrees with ECDH_P384 keys.
#   WINE=/opt/wine-sg/bin/wine test/bcryptkdf-gate.sh   (mutants SG_MUTANT_KDF_CONCAT_NO_OTHERINFO, SG_MUTANT_ECDH_CURVE_IGNORED)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: python3 missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-bcryptkdf.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/bcryptkdf-probe.c" -lbcrypt 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
python3 -I - "$T/out" > "$T/expected" <<'EOP'
import hashlib, hmac, sys
out = dict(l.split('=', 1) for l in open(sys.argv[1]).read().split('\n') if '=' in l)
def concat(h, z, other, n):
    r, i = b'', 1
    while len(r) < n:
        r += hashlib.new(h, i.to_bytes(4, 'big') + z + other).digest(); i += 1
    return r[:n]
def sp800108(h, key, label, context, n):
    r, i = b'', 1
    while len(r) < n:
        r += hmac.new(key, i.to_bytes(4, 'big') + label + b'\0' + context + (n * 8).to_bytes(4, 'big'), h).digest(); i += 1
    return r[:n]
print('pbkdf2=' + hashlib.pbkdf2_hmac('sha256', b'password', b'salt', 4096, 40).hex())
print('pbkdf2_sha3=' + hashlib.pbkdf2_hmac('sha3_256', b'password', b'salt', 4096, 32).hex())
print('hkdf=3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865')
print('concat=' + concat('sha512', b'shared secret Z', b'AESAliceBob', 80).hex())
print('sp800108=' + sp800108('sha256', b'key derivation key', b'label', b'context', 48).hex())
print('pbkdf2_pseudo=' + hashlib.pbkdf2_hmac('sha256', b'password', b'salt', 4096, 40).hex())
print('bad_hash=0xc00000bb')
print('hkdf_unfinalized=0xc000000d')
print('ecdh_concat=0 len 48')
print('ecdh_both=1')
raw = bytes.fromhex(out.get('ecdh_raw', ''))
print('ecdh_kdf=' + concat('sha256', raw[::-1], b'AESAliceBob', 48).hex())
print('ecdh_open=0')
print('ecdh_badcurve=0xc00000bb')
print('ecdh_curve=0')
print('ecdh_generate=0')
print('ecdh_keysize=48')
print('ecdh_import_generic=0')
print('ecdh_generic_agree=1 len 48')
print('ecdh_bysize=0 32')
print('ecdh_nosize=0xc000000d')
print('done=1')
EOP
while IFS= read -r line; do
    key=${line%%=*}
    if grep -qxF "$line" "$T/out"; then pass "$key"
    else fail "$key: wanted '${line#*=}', got '$(grep "^$key=" "$T/out" | cut -d= -f2- || echo none)'"; fi
done < "$T/expected"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
