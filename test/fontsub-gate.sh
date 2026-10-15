#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# fontsub: CreateFontPackage subsets a TrueType font (patch 5534). The kept
# glyphs (a character list through a cmap, or glyph indices, plus composite
# components and glyph 0) keep their indices; the others become empty. The
# output is a working font: sorted table directory, whole-file checksum,
# long loca, loadable. A TTC face is picked by index; font packages
# (TTFCFP_SUBSET1/DELTA) are refused.
#
#   WINE=/opt/wine-sg/bin/wine test/fontsub-gate.sh
# Mutant: SG_MUTANT_FONTSUB_NOCLOSURE (dlls/fontsub: composite components are not kept).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINESERVER
RC=0
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-fontsub.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
mkdir -p "$HOME"

cat > "$T/expect" <<'X'
fixture loadable=1
char C ret=0 sum=ok sorted=1 glyphs=5 loca=1 kept: 0 1 3 loadable=1
sizes 1
glyph 3 ret=0 sum=ok sorted=1 glyphs=5 loca=1 kept: 0 1 3 loadable=1
glyph 4 ret=0 sum=ok sorted=1 glyphs=5 loca=1 kept: 0 4 loadable=1
chars B D any-encoding ret=0 sum=ok sorted=1 glyphs=5 loca=1 kept: 0 2 4 loadable=1
no such cmap ret=1000
no subset flag ret=0 same=1
package format ret=1000
truncated ret=1000
garbage ret=1000
ttc face 1 char D ret=0 sum=ok sorted=1 glyphs=5 loca=1 kept: 0 4 loadable=1
ttc bad index ret=1000
X

for a in 64:x86_64 32:i686; do
    b=${a%%:*}; cc=${a##*:}-w64-mingw32-gcc
    "$cc" -O2 -o "$T/probe$b.exe" "$HERE/fontsub-probe.c" -lgdi32 || { echo "FAIL  build $b"; exit 1; }
done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for b in 64 32; do
    cp "$T/probe$b.exe" "$WINEPREFIX/drive_c/"
    (cd "$WINEPREFIX/drive_c" && DISPLAY= timeout -s KILL 120 "$WINE" "./probe$b.exe" >"$T/out$b.txt" 2>/dev/null </dev/null)
    tr -d '\r' <"$T/out$b.txt" | grep -v 'XDG_RUNTIME' >"$T/got$b.txt"
    if cmp -s "$T/expect" "$T/got$b.txt"; then
        echo "PASS  $b-bit: subsets by characters and glyphs, closes composites, refuses packages, loads"
    else
        echo "FAIL  $b-bit output differs:"; diff "$T/expect" "$T/got$b.txt" | head -12; RC=1
    fi
done
[ $RC = 0 ] && echo "fontsub-gate: all passed" || echo "fontsub-gate: FAILURES"
exit $RC
