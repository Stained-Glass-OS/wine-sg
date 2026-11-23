#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# t2embed: TTEmbedFont, TTEmbedFontEx and TTEmbedFontFromFileA with TTEMBED_SUBSET embed a subset font
# (patches/sg/9502; the whole font was embedded): the requested characters' glyphs (through the format 4
# or format 12 cmap, or glyph indices), their composite components and glyph 0 keep their outlines and
# indices, the other glyphs are emptied (fontsub CreateFontPackage). The EOT is flagged subsetted, loads
# with TTLOAD_FONT_SUBSETTED and draws the kept character pixel for pixel like the original, the others
# not at all. A font whose fsType forbids subsetting is embedded whole with TTEMBED_SUBSETCANCEL.
#   WINE=/opt/wine-sg/bin/wine test/eotsubset-gate.sh
# Mutant: SG_MUTANT_EOT_NO_SUBSET (dlls/t2embed/main.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${EOTSUBSET_DPY:-322}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

[ -x /usr/bin/python3 ] && /usr/bin/python3 -c 'import fontTools' 2>/dev/null || { echo "SKIP: python3-fonttools missing"; exit 77; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-eotsubset.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/eotsubset-probe.c" -lgdi32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
/usr/bin/python3 -I "$HERE/eotsubset-font.py" "$T/font.ttf" "SG Subset" 0 &&
    /usr/bin/python3 -I "$HERE/eotsubset-font.py" "$T/nosub.ttf" "SG NoSubset" 0x100 ||
    { echo "FAIL  the test fonts were not made"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
cd "$T" && timeout -s KILL 60 "$WINE" "$T/probe.exe" "Z:$T/font.ttf" "Z:$T/nosub.ttf" "Z:$T/chars.eot" "Z:$T/ex.eot" "Z:$T/glyphs.eot" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1
for c in "chars.eot .notdef A C bar" "ex.eot .notdef D" "glyphs.eot .notdef B"; do
    set -- $c
    f=$1; shift
    if [ -s "$T/$f" ]; then
        /usr/bin/python3 -I "$HERE/eotsubset-check.py" "$T/font.ttf" "$T/$f" "$@" >"$T/check" 2>&1 || rc=1
        tail -3 "$T/check"
    else echo "FAIL  no $f"; rc=1; fi
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
