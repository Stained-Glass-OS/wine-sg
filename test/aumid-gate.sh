#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A program's application user model ID (patches/sg/0423):
# GetCurrentApplicationUserModelId and GetApplicationUserModelId exist --
# Thunderbird 128 calls the first at start and after mail arrives, and died
# on the missing export. A program outside any package gets
# APPMODEL_ERROR_NO_APPLICATION (15703), as on Windows; one inside a deployed
# package gets "<family>!<app>" from the package's app execution alias.
#
#   WINE=/opt/wine-sg/bin/wine test/aumid-gate.sh
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
T=$(mktemp -d /var/tmp/sg-aumid.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/aumid-probe.exe" "$HERE/aumid-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/aumid-probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -qx "$1"; }
has 'UNPACKAGED 15703' && pass "a program outside any package: APPMODEL_ERROR_NO_APPLICATION" || fail "unpackaged: $(printf '%s\n' "$out" | head -1)"
has 'SELF 15703' && pass "GetApplicationUserModelId of itself: the same" || fail "self"
has 'OFPROCESS 0 SgTest.Aumid_8wekyb3d8bbwe!App' && pass "another process, inside a deployed package: its AUMID" || fail "of process: $(printf '%s\n' "$out" | grep OFPROCESS)"
has 'CHILD 0 SgTest.Aumid_8wekyb3d8bbwe!App' && pass "the packaged program's own AUMID" || fail "child: $(printf '%s\n' "$out" | grep '^CHILD ')"
has 'CHILD_SMALL 122 31' && pass "a short buffer: ERROR_INSUFFICIENT_BUFFER and the length needed" || fail "small: $(printf '%s\n' "$out" | grep CHILD_SMALL)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
