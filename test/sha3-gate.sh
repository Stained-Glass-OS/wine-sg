#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# bcrypt SHA-3 (6087): BCryptOpenAlgorithmProvider("SHA3-256"/"SHA3-384"/"SHA3-512") was "not supported"
# (LibreOffice's xmlsec CNG back end asks for them at start). Our own Keccak-f[1600] sponge from FIPS 202:
# one-shot and incremental hashes, HMAC (the HMAC block is the sponge's rate), the SHA3 and HMAC-SHA3
# pseudo-handles, and HashBlockLength. Expected values are Python hashlib's.
#   WINE=/opt/wine-sg/bin/wine test/sha3-gate.sh   (mutant SG_MUTANT_SHA3_WRONG_RATE)
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
T=$(mktemp -d /var/tmp/sg-sha3.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/sha3-probe.c" -lbcrypt 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
python3 -I - > "$T/expected" <<'EOP'
import hashlib, hmac
for n in (256, 384, 512):
    h = 'sha3_%d' % n
    print('open_%d=0' % n)
    print('length_%d=%d' % (n, n // 8))
    print('block_%d=%d' % (n, 200 - n // 4))
    print('abc_%d=%s' % (n, hashlib.new(h, b'abc').hexdigest()))
    print('long_%d=%s' % (n, hashlib.new(h, b'a' * 1000).hexdigest()))
    print('pseudo_%d=%s' % (n, hashlib.new(h, b'abc').hexdigest()))
    print('hmac_%d=%s' % (n, hmac.new(b'key', b'The quick brown fox', h).hexdigest()))
    print('hmac_pseudo_%d=%s' % (n, hmac.new(b'key', b'The quick brown fox', h).hexdigest()))
print('done=1')
EOP
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
while IFS= read -r line; do
    key=${line%%=*}
    if grep -qxF "$line" "$T/out"; then pass "$key"
    else fail "$key: wanted '${line#*=}', got '$(grep "^$key=" "$T/out" | cut -d= -f2- || echo none)'"; fi
done < "$T/expected"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
