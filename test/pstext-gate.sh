#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Text printed through the PostScript driver stays text (patches/sg/9759, i0993). Print to PDF
# (wineps PostScript -> ghostscript pdfwrite, the cups-pdf/CUPS path) gave PDFs whose text could
# not be searched or copied: the downloaded Type 42 / Type 1 fonts said nothing about which
# character each glyph is, so the PDF fonts had no ToUnicode map. Windows' PostScript driver writes
# /GlyphNames2Unicode in each font's FontInfo; so do we. test/pstext-probe.c prints the same lines
# to a Type 42 printer and a Type 1 printer; ghostscript makes PDFs; pdftotext must read the lines.
#   WINE=/opt/wine-sg/bin/wine test/pstext-gate.sh
# Glyph-index text (ETO_GLYPH_INDEX without widths) was spread out: the spooled metafile measured
# the glyph indices as characters (patches/sg/9760).
# Mutants: SG_MUTANT_PS_NO_G2U (wineps download.c), SG_MUTANT_EMF_GLYPH_AS_CHAR (gdi32 emfdc.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in gs pdftotext pdffonts "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pstext.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    sg_kill_prefix "$T/prefix"
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -O1 -o "$T/probe.exe" "$HERE/pstext-probe.c" -lwinspool -lgdi32 || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
export CUPS_SERVER="$T/no-cups.sock"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
"$WINESERVER" -w
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_tt_font=1' && { echo "SKIP: no TrueType font on this system"; exit 77; }
printf '%s\n' "$out" | grep -Eq '^no_(driver|printer|dc)' && { fail "the printers could not be set up"; exit 1; }
cff=1; printf '%s\n' "$out" | grep -q '^no_cff_font=1' && cff=0
for kind in 42 1; do
    PS="$WINEPREFIX/drive_c/out$kind.ps"
    [ -s "$PS" ] || { fail "Type $kind: no PostScript"; continue; }
    gs -q -dNOPAUSE -dBATCH -dSAFER -sDEVICE=pdfwrite -sOutputFile="$T/out$kind.pdf" "$PS" >"$T/gs$kind.log" 2>&1
    grep -qi error "$T/gs$kind.log" && fail "Type $kind: ghostscript reported errors: $(head -3 "$T/gs$kind.log")"
    [ -s "$T/out$kind.pdf" ] || { fail "Type $kind: no PDF"; continue; }
    txt=$(pdftotext -enc UTF-8 "$T/out$kind.pdf" - 2>/dev/null)
    printf '%s\n' "$txt" | grep -v '^[[:space:]]*$' | sed "s/^/      type $kind: /"
    printf '%s\n' "$txt" | grep -q 'TrueTypeLine searchable words' && pass "Type $kind: TrueType text is text" ||
        fail "Type $kind: TrueType text cannot be read back"
    printf '%s\n' "$txt" | grep -q 'GlyphIndexLine shaped words' && pass "Type $kind: text given as glyph indices is text" ||
        fail "Type $kind: glyph-index text cannot be read back"
    printf '%s\n' "$txt" | grep -q 'Grüße café €5' && pass "Type $kind: characters outside ASCII are read back" ||
        fail "Type $kind: characters outside ASCII cannot be read back"
    if [ $cff = 1 ]; then
        printf '%s\n' "$txt" | grep -q 'CffFontLine outline words' && pass "Type $kind: OpenType CFF text is text" ||
            fail "Type $kind: CFF text cannot be read back"
    fi
    # the fonts stay embedded (the text is not just put in a standard font)
    emb=$(pdffonts "$T/out$kind.pdf" 2>/dev/null | awk 'NR>2 && $(NF-4)=="yes"' | wc -l)
    [ "$emb" -ge 1 ] && pass "Type $kind: fonts are embedded ($emb)" || fail "Type $kind: no embedded font"
done
printf '%s\n' "$out" | grep -q '^emf_dx_ok=1' && pass "a metafile records glyph-index text with the glyphs' advances" ||
    fail "a metafile records glyph-index text with the wrong advances (the glyph indices measured as characters)"
# the page still looks the same: ghostscript renders the job with ink where the first line is
gs -q -dNOPAUSE -dBATCH -dSAFER -sDEVICE=pnggray -r50 -sOutputFile="$T/page.png" "$WINEPREFIX/drive_c/out42.ps" >/dev/null 2>&1
if command -v convert >/dev/null; then
    ink=$(convert "$T/page.png" -crop 300x20+25+25 -negate -threshold 50% -format "%[fx:mean*w*h]" info: 2>/dev/null)
    [ "${ink:-0}" -gt 50 ] 2>/dev/null && pass "the text is drawn ($ink dark pixels)" || fail "the text is not drawn (${ink:-none})"
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
