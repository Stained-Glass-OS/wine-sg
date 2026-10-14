#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msiexec /f with a product code (patches/sg/5521).
#
# msiexec /f accepts the path of a package or the product code of an
# installed product; the code form only logged a FIXME and did nothing.
# MsiReinstallProduct takes a product code, so the code is handed to it.
# The gate follows msiexec's calls with +msi: `msiexec /fa {code}` must call
# MsiReinstallProductW with that code, and a code that is not installed must
# make msiexec fail (rc 1605 & 0xff = 69, ERROR_UNKNOWN_PRODUCT).
#
#   WINE=/opt/wine-sg/bin/wine test/msiexec-repair-gate.sh
# Mutant (programs/msiexec/msiexec.c; x86_64_CFLAGS / i386_CFLAGS = "-g -O2 -DSG_MUTANT_MSIEXEC_NOCODE"):
# the product code form does nothing.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINESERVER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-msirepair.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

CODE='{3B8E0B7A-3F3A-4C8B-9E36-5A1F0C2D7E41}'
DISPLAY= WINEDEBUG=+msi timeout -s KILL 120 "$WINE" msiexec /fa "$CODE" /qn > "$T/out" 2>&1
RET=$?
grep -q "trace:msi:MsiReinstallProductW L\"$CODE\", 0x40" "$T/out" \
    && pass "msiexec /fa PRODUCT-CODE reinstalls the product through MsiReinstallProduct, all files (0x40)" \
    || fail "no MsiReinstallProductW call: $(grep -c 'trace:msi' "$T/out") msi lines"
[ "$RET" = 69 ] && pass "a product that is not installed fails (rc 1605 & 0xff)" || fail "rc $RET for an unknown product"

DISPLAY= WINEDEBUG=+msi timeout -s KILL 120 "$WINE" msiexec /fu "$CODE" /qn > "$T/out2" 2>&1
grep -q "trace:msi:MsiReinstallProductW L\"$CODE\", 0x" "$T/out2" && pass "other repair options (/fu) are passed on as well" || fail "/fu: no call"

[ $RC = 0 ] && echo "msiexec-repair-gate: all passed" || echo "msiexec-repair-gate: FAILURES"
exit $RC
