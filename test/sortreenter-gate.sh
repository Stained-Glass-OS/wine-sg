#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A locale's sort looked up re-entrantly (patches/sg/0513). kernelbase reads
# the sort's id from the registry the first time a locale's sort is needed;
# App-V's virtual registry (Click-to-Run Office) hooks the registry and
# lower-cases key names with LCMapString, which needs the same sort again:
# the lookup recursed until the stack ran out (Word, on "Sign in" with the
# older sign-in). The probe hooks kernelbase's NtOpenKeyEx the same way: the
# nested lookup must not recurse, the outer one must still succeed, and the
# sort must be kept for later calls.
#
#   WINE=/opt/wine-sg/bin/wine test/sortreenter-gate.sh
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
T=$(mktemp -d /var/tmp/sg-sortreenter.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/sortreenter-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
[ "$(v import)" = 1 ] && pass "kernelbase imports NtOpenKeyEx (the probe can hook it)" || fail "import: $(v import)"
[ "$(v hooked)" = 1 ] && pass "the sort lookup went through the hooked registry" || fail "hooked: $(v hooked)"
[ "$(v maxdepth)" = 1 ] && pass "the hook's nested lookup did not recurse" || fail "maxdepth: $(v maxdepth)"
[ "$(v result)" = "6 hello" ] && pass "the outer lower-casing succeeded" || fail "result: $(v result)"
[ "$(v again)" = "6 HELLO" ] && pass "the locale's sort is kept for later calls" || fail "again: $(v again)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
