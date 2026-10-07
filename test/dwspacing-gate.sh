#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A glyph DirectWrite draws at a fraction of a pixel lands at that fraction
# (patches/sg/1350). Chromium's text -- VS Code, Chrome, Edge, every Electron
# program -- is drawn by Skia one glyph at a time: Skia asks
# IDWriteGlyphRunAnalysis for the glyph at a quarter-pixel offset (0, 1/4,
# 1/2, 3/4; in the run's transform) and places the image at the whole pixel.
# Wine truncated the offset, so each glyph landed up to 3/4 pixel left of its
# place, each by another amount: VS Code read "Fun dam entals", "Custom ize".
#
# test/dwspacing-probe.c, 64- and 32-bit, for Liberation Sans (TrueType) and
# Inter (CFF), at 12, 13, 16 and 21 px:
#   * a glyph's ink moves with its origin's fraction (1/4, 1/2, 3/4 px; the
#     baseline origin), each within 0.25 px;
#   * "Fundamentals" laid out as Skia lays it out (design advances; each
#     glyph's image at its quarter pixel, through the transform): every
#     glyph's ink centre within 0.3 px of where its advance puts it.
#
#   WINE=/opt/wine-sg/bin/wine test/dwspacing-gate.sh
# Mutation: -DSG_MUTANT_DWSPACING in dlls/dwrite/font.c and freetype.c fails it.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
FONTS=""
for f in /usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf \
         /usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf \
         /usr/share/fonts/opentype/inter/Inter-Regular.otf; do
    [ -f "$f" ] && FONTS="$FONTS $f"
done
[ -n "$FONTS" ] || { echo "SKIP: neither Liberation Sans nor Inter is installed"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dwspacing.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for m in "$MINGW:64" "$MINGW32:32"; do
    TMPDIR=/var/tmp "${m%:*}" -municode -O2 -o "$T/probe${m#*:}.exe" "$HERE/dwspacing-probe.c" -ldwrite ||
        { fail "probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for b in 64 32; do
    for f in $FONTS; do
        name=$(basename "$f")
        winpath="Z:$(printf '%s' "$f" | tr / '\\')"
        for px in 12 13 16 21; do
            out=$(cd "$T" && timeout -s KILL 60 "$WINE" "$T/probe$b.exe" "$winpath" "$px" 2>/dev/null | tr -d '\r')
            case "$out" in *"face OK"*) ;; *) fail "$b-bit $name: no face ($out)"; continue ;; esac
            shift=$(printf '%s\n' "$out" | sed -n 's/^shift WORST //p')
            err=$(printf '%s\n' "$out" | sed -n 's/^layout MAXERR //p')
            if awk -v s="$shift" 'BEGIN { exit !(s != "" && s + 0 <= 0.25) }'; then
                pass "$b-bit $name ${px}px: the ink moves with the origin's fraction (worst off by $shift px)"
            else
                fail "$b-bit $name ${px}px: the origin's fraction is lost (worst off by $shift px)"
                printf '%s\n' "$out" | grep '^shift' | sed 's/^/      /'
            fi
            if awk -v e="$err" 'BEGIN { exit !(e != "" && e + 0 <= 0.3) }'; then
                pass "$b-bit $name ${px}px: Skia's layout keeps every glyph at its place (worst $err px)"
            else
                fail "$b-bit $name ${px}px: Skia's layout puts a glyph $err px off its place"
                printf '%s\n' "$out" | grep '^gaps' | sed 's/^/      /'
            fi
        done
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
