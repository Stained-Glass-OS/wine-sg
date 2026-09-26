#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ucrtbase's imaxdiv, wcstoimax, wcstoumax, _wcstoimax_l and _wcstoumax_l
# (patches/sg/0421) -- stubs in Wine 10.0, so a program built with a current
# MSVC that uses <inttypes.h> died on the first call. In ucrtbase, msvcr120
# and msvcr120_app; 64- and 32-bit.
#
#   WINE=/opt/wine-sg/bin/wine test/crtimax-gate.sh
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
T=$(mktemp -d /var/tmp/sg-crtimax.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$T/p64.exe" "$HERE/crtimax-probe.c" &&
    i686-w64-mingw32-gcc -O2 -o "$T/p32.exe" "$HERE/crtimax-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
EXPECT='IMAXDIV -2333333333333 -1
WCSTOIMAX -9000000000123 rest=xyz
WCSTOUMAX 18446744073709551615
WCSTOIMAX_L 123456789012
WCSTOUMAX_L 511'
for run in "p64 ucrtbase.dll" "p64 msvcr120.dll" "p64 msvcr120_app.dll" "p32 ucrtbase.dll" "p32 msvcr120.dll"; do
    set -- $run
    out=$(timeout -s KILL 60 "$WINE" "$T/$1.exe" "$2" 2>/dev/null </dev/null | tr -d '\r')
    if [ "$out" = "$EXPECT" ]; then pass "$2 ($1): imaxdiv and the wide intmax conversions answer"
    else fail "$2 ($1): $(printf '%s' "$out" | tr '\n' ';')"; fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
