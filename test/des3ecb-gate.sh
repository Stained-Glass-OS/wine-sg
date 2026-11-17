#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# 3DES in ECB mode (7819). BCryptSetProperty(BCRYPT_CHAINING_MODE, ECB) on a 3DES algorithm said
# STATUS_NOT_SUPPORTED, so .NET's TripleDES in ECB mode threw a CryptographicException: Remote Desktop
# Manager could not save its settings at its first start. Now ECB is made from CBC with the vector reset
# for each block, as for AES. The gate checks the mode is set and read back, NIST SP 800-67's example
# encrypts to its known answer and decrypts back, two equal blocks encrypt alike, and a vector is refused.
#   WINE=/opt/wine-sg/bin/wine test/des3ecb-gate.sh   (mutant SG_MUTANT_3DES_NO_ECB, bcrypt_main.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-des3ecb.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/des3ecb-probe.c" -lbcrypt || { fail "the probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" probe.exe 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() { printf '%s\n' "$out" | grep -qxF "$1=$2" && pass "$3" || fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; }
expect SetMode 0 "BCryptSetProperty(BCRYPT_CHAINING_MODE, ECB) on 3DES succeeds"
expect Mode ChainingModeECB "and the mode reads back"
expect Encrypt "0 24 1" "NIST SP 800-67's example encrypts to its known answer"
expect Decrypt "0 1" "and decrypts back"
expect Equal "0 1" "two equal blocks encrypt alike (ECB)"
expect WithIV 0xc000000d "a vector in ECB mode: STATUS_INVALID_PARAMETER"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
