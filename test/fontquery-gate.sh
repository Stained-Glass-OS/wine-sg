#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DirectWrite font set queries (patches/sg/1621), 64- and 32-bit:
# test/fontquery-probe.c. Font sets answered E_NOTIMPL past listing their
# fonts: no property values (weight/stretch/style not at all), no matching
# by weight/stretch/style, no filters, no font faces, and the builder took
# neither font sets nor fonts with properties of their own (text layout in
# Office, WinUI and Chromium-based apps asks these).
#
#   WINE=/opt/wine-sg/bin/wine test/fontquery-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/dwrite/font.c): SG_MUTANT_NO_FONTSET_PROPERTIES,
# SG_MUTANT_NO_FONTSET_METHODS, SG_MUTANT_NO_FONT_DISTANCE,
# SG_MUTANT_NO_GIVEN_NAMES.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fontquery.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/fontquery-probe.c" -ldwrite -luuid &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/fontquery-probe.c" -ldwrite -luuid || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
