#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# RtlValidRelativeSecurityDescriptor (patches/sg/0507). It compared
# RtlValidSecurityDescriptor's BOOLEAN with STATUS_SUCCESS, so it refused
# every valid descriptor. The offline registry library (offreg) checks each
# hive's security cells with it: no hive would open -- not even one it had
# just written -- and Click-to-Run rebuilt Office's virtual registry hives
# empty. A valid self-relative descriptor passes; truncated ones, a part
# outside the descriptor, an absolute descriptor, and one lacking a part the
# caller requires are refused.
#
#   WINE=/opt/wine-sg/bin/wine test/relsd-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-relsd.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/relsd-probe.c" -ladvapi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
[ "$(v valid)" = 1 ] && pass "a valid self-relative descriptor (owner, group, DACL) is valid" || fail "valid: $(v valid)"
[ "$(v truncated)" = 0 ] && [ "$(v tiny)" = 0 ] && pass "a truncated descriptor is not" || fail "truncated: $(v truncated) $(v tiny)"
[ "$(v badowner)" = 0 ] && pass "an owner outside the descriptor is not" || fail "bad owner: $(v badowner)"
[ "$(v absolute)" = 0 ] && pass "an absolute (not self-relative) descriptor is not" || fail "absolute: $(v absolute)"
[ "$(v nodacl-asked)" = "0 nodacl-notasked 1" ] && pass "a missing part matters only when the caller requires it" || fail "required parts: $(v nodacl-asked)"
[ "$(v null)" = 0 ] && pass "no descriptor is not valid" || fail "null: $(v null)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
