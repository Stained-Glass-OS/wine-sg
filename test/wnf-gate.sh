#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# NtQueryWnfStateData exists (patches/sg/0433): Thunderbird 128 aborted on
# the missing export right after downloading mail, when it asked for the
# quiet-hours (Focus Assist) state before announcing it. No Windows
# Notification Facility states are published: a state reads as empty
# (change stamp 0, no data), which means "not quiet".
#
#   WINE=/opt/wine-sg/bin/wine test/wnf-gate.sh
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
T=$(mktemp -d /var/tmp/sg-wnf.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/wnf-probe.exe" "$HERE/wnf-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 60 "$WINE" "$T/wnf-probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -qx "$1"; }
has 'EXPORTED 1 1' && pass "ntdll exports NtQueryWnfStateData and ZwQueryWnfStateData" || fail "exports: $(printf '%s\n' "$out" | head -1)"
has 'QUERY 00000000 stamp 0 size 0' && pass "a state reads as published and empty (not quiet)" || fail "query: $(printf '%s\n' "$out" | grep QUERY)"
has 'NULLNAME c000000d' && pass "no state name: STATUS_INVALID_PARAMETER" || fail "null name: $(printf '%s\n' "$out" | grep NULLNAME)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
