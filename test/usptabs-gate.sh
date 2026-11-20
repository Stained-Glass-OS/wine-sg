#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Far-edge tab stops (negative SCRIPT_TABDEF.pTabStops) in ScriptStringAnalyse
# (patches/sg/8223): the text after the tab ends at the stop. test/usptabs-probe.c,
# 64- and 32-bit. They were near stops with a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/usptabs-gate.sh
# Mutant: SG_MUTANT_USP_NEAR_TABS_ONLY (gdi32/uniscribe/usp10.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-usptabs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/usptabs-probe.c" -lusp10 -lgdi32 &&
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/usptabs-probe.c" -lusp10 -lgdi32 ||
    { fail "the probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for b in p64 p32; do
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$b.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -q '^done=1' || { fail "$b: the probe did not finish"; continue; }
    for k in far_end near_start mixed_far far_too_wide; do
        printf '%s\n' "$out" | grep -q "^$k=1" && pass "$b: $k" || fail "$b: $k ($(printf '%s\n' "$out" | grep "^$k=" || echo none))"
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
