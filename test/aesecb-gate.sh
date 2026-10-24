#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# AES-ECB through bcrypt (6091): ECB takes no IV, yet every ECB BCryptEncrypt/BCryptDecrypt logged
# "FIXME: Unexpected vector len 0" (KeePass 2's AES-KDF: dozens per database open). The output was right; the
# FIXME was for a chaining vector ECB never has. Checked: the FIPS-197 AES-128 example, one block and three,
# decrypted back, and no FIXME from bcrypt.
#   WINE=/opt/wine-sg/bin/wine test/aesecb-gate.sh   (mutant SG_MUTANT_ECB_VECTOR_FIXME)
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
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-aesecb.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/aesecb-probe.c" -lbcrypt 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
WINEDEBUG=-all,fixme+bcrypt timeout -s KILL 120 "$WINE" "$T/probe.exe" > "$T/out" 2> "$T/err" </dev/null
out=$(tr -d '\r' < "$T/out")
printf '%s\n' "$out" | sed 's/^/      /'
c=69c4e0d86a7b0430d8cdb78070b4c55a
printf '%s\n' "$out" | grep -qx "ecb_one=$c" && pass "one block: the FIPS-197 ciphertext" || fail "one block: $(printf '%s\n' "$out" | grep ecb_one)"
printf '%s\n' "$out" | grep -qx "ecb_three=$c$c$c" && pass "three blocks, each alone (ECB)" || fail "three blocks: $(printf '%s\n' "$out" | grep ecb_three)"
printf '%s\n' "$out" | grep -qx "ecb_back=0 1" && pass "decrypted back" || fail "decrypt: $(printf '%s\n' "$out" | grep ecb_back)"
if grep -q "Unexpected vector len" "$T/err"; then fail "bcrypt still logs: $(grep -m1 'Unexpected vector len' "$T/err")"
else pass "no 'Unexpected vector len' FIXME for ECB"; fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
