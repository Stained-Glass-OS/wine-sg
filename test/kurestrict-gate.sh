#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# crypt32's key usage restriction extension (szOID_KEY_USAGE_RESTRICTION 2.5.29.4,
# X509_KEY_USAGE_RESTRICTION; patches/sg/9654). AutoHotkey 2's setup makes a certificate with it to
# sign AutoHotkey*_UIA.exe; Wine had no encoder ("Unsupported encoder for lpszStructType
# 2.5.29.4"), so setup stopped at an error box and a silent install never ended. The probe
# (test/kurestrict-probe.c) encodes AutoHotkey's value and one with two policies, and decodes it:
#   AutoHotkey's: 30 04 03 02 00 90; with policies: SEQUENCE { SEQUENCE OF SEQUENCE OF OID, BIT STRING }
#   decoded again it is the same; an empty value is an empty SEQUENCE
#   WINE=/opt/wine-sg/bin/wine test/kurestrict-gate.sh
# Mutant (crypt32/encode.c): SG_MUTANT_NO_KEY_USAGE_RESTRICTION.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kurestrict.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/kurestrict-probe.c" -lcrypt32 || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '      %s\n' "$out"
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
v() { printf '%s\n' "$out" | awk -v k="$1" '$1 == k { print $2; exit }'; }
[ "$(v AHK)" = 300403020090 ] && pass "AutoHotkey's key usage restriction encodes (30 04 03 02 00 90)" || fail "AutoHotkey's: '$(v AHK)'"
# SEQUENCE { SEQUENCE OF { SEQUENCE OF { 1.3.6.1.4.1.311.10.3.1, 1.3.6.1.5.5.7.3.3 }, SEQUENCE OF { 2.5.29.32.0 } }, BIT STRING 80 }
P=302630203016060a2b0601040182370a030106082b0601050507030330060604551d200003020080
[ "$(v POL)" = "$P" ] && pass "with policies: SEQUENCE OF SEQUENCE OF OID, then the key usage" || fail "with policies: '$(v POL)' (want $P)"
[ "$(v ROUND)" = ok ] && pass "it decodes to the same value" || fail "decoded: '$(v ROUND)'"
[ "$(v NONE)" = 3000 ] && pass "an empty value is an empty SEQUENCE" || fail "empty: '$(v NONE)'"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
