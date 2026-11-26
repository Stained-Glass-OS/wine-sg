#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 9411 (issue #15, "emojis just show as boxes"): DirectWrite's
# system font fallback had no entry for emoji or symbols, so every DirectWrite
# program -- Chromium-based browsers (Skia), WinUI, Direct2D text -- drew emoji
# as missing-glyph boxes. As on Windows, emoji fall back to Segoe UI Emoji and
# symbols to Segoe UI Symbol; where those are not installed, to the colour
# emoji font Stained Glass ships (Noto Color Emoji), then Symbola. And a CBDT
# font's 32-bit strikes are PNG images (image formats 17-19), not BGRA: Direct2D
# took Noto Color Emoji for no colour font and drew nothing at all.
#
# Checks: the fallback maps U+1F600, U+1F44D, U+2603 to Noto Color Emoji (PNG
# glyphs) and U+1D400 to Symbola; text layout gives the emoji its width (a bitmap-only
# glyph had no advance); U+2764 U+FE0F is one emoji from the emoji font (the selector
# made the base font give up the heart; a missing selector is invisible); Direct2D DrawText (colour fonts on) of
# U+1F600 in "Segoe UI" draws coloured pixels; "A" still draws, in black.
# Needs Noto Color Emoji (fonts-noto-color-emoji, or SG_EMOJI_FONT_DIR, a folder
# with NotoColorEmoji.ttf) and Symbola (fonts-symbola); skips (77) without.
#   WINE=... test/emojifallback-gate.sh
# Mutants: SG_MUTANT_NO_EMOJI_FALLBACK (dlls/dwrite/analyzer.c),
#          SG_MUTANT_CBLC_BY_DEPTH (dlls/dwrite/opentype.c),
#          SG_MUTANT_BITMAP_ADVANCE_ZERO (dlls/dwrite/freetype.c),
#          SG_MUTANT_NO_EMOJI_PRESENTATION (dlls/dwrite/analyzer.c),
#          SG_MUTANT_IGNORABLE_NOTDEF (dlls/dwrite/opentype.c).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=$(sg_wineserver)
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
W=$(mktemp -d /var/tmp/sg-emojifallback.XXXXXX)
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
cleanup() { WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; sleep 1; rm -rf "$W"; }
trap cleanup EXIT
if [ -n "${SG_EMOJI_FONT_DIR:-}" ]; then
    printf '<?xml version="1.0"?>\n<fontconfig><include ignore_missing="yes">/etc/fonts/fonts.conf</include><dir>%s</dir><cachedir>%s/fc</cachedir></fontconfig>\n' \
        "$SG_EMOJI_FONT_DIR" "$W" > "$W/fonts.conf"
    export FONTCONFIG_FILE="$W/fonts.conf"
fi
fc-list 2>/dev/null | grep -qi 'Noto Color Emoji' || { echo "SKIP: no Noto Color Emoji (fonts-noto-color-emoji or SG_EMOJI_FONT_DIR)"; exit 77; }
fc-list 2>/dev/null | grep -qi 'Symbola' || { echo "SKIP: no Symbola (fonts-symbola)"; exit 77; }
sg_gate_display || exit $?
"$MINGW" -O1 -o "$W/probe.exe" "$HERE/emojifallback-probe.c" -ldwrite -ld2d1 -lole32 -lwindowscodecs -luuid || { echo "FAIL build"; exit 1; }
export WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$W/probe.exe" map 1F600 1F44D 2603 1D400 2764+FE0F 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/  /'
for cp in 1F600 1F44D 2603; do
    grep -q "^MAP $cp Noto Color Emoji png, width " <<<"$out" && pass "U+$cp falls back to the colour emoji font, as PNG glyphs" \
        || fail "U+$cp: $(grep "MAP $cp " <<<"$out")"
done
w=$(sed -n 's/^MAP 1F600 .* width \([0-9]*\) .*/\1/p' <<<"$out")
# Noto Color Emoji's advance is 2550/2048 em: 125 at 100 px. Bitmap-only glyphs had none (0):
# text layout stacked an emoji on the next character and centred ones half their width off
[ "${w:-0}" -ge 110 ] && [ "${w:-0}" -le 140 ] && pass "text layout gives the emoji its advance ($w at 100 px)" \
    || fail "the emoji's width in text layout: '${w:-none}' (want about 125)"
# red heart, U+2764 U+FE0F: the base font has a text heart, the selector asks for the emoji -- the
# emoji font takes both, and the selector takes no room (it was a box of its own, 249 wide)
l=$(grep '^MAP 2764+FE0F ' <<<"$out")
w=$(sed -n 's/.* width \([0-9]*\) .*/\1/p' <<<"$l")
case "$l" in "MAP 2764+FE0F Noto Color Emoji png, width "*" mapped 2 "*) ok=1 ;; *) ok=0 ;; esac
[ "$ok" = 1 ] && [ "${w:-0}" -ge 110 ] && [ "${w:-0}" -le 140 ] \
    && pass "a character with the emoji presentation selector (red heart) is the emoji font's, one emoji wide" \
    || fail "red heart (U+2764 U+FE0F): '$l'"
grep -q '^MAP 1D400 Symbola ' <<<"$out" && pass "a mathematical letter (U+1D400) falls back to a symbol font (Symbola)" \
    || fail "U+1D400: $(grep 'MAP 1D400' <<<"$out")"
out=$(timeout -s KILL 120 "$WINE" "$W/probe.exe" draw 1F600 41 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/  /'
read -r _ _ _ ink _ colour <<<"$(grep '^DRAW 1F600 ' <<<"$out")"
[ "${ink:-0}" -gt 200 ] && [ "${colour:-0}" -gt 200 ] && pass "Direct2D draws the emoji in colour ($ink pixels, $colour coloured)" \
    || fail "Direct2D emoji: $(grep 'DRAW 1F600' <<<"$out")"
read -r _ _ _ ink _ colour <<<"$(grep '^DRAW 41 ' <<<"$out")"
[ "${ink:-0}" -gt 200 ] && [ "${colour:-1}" -eq 0 ] && pass "and plain text still in its brush's black" || fail "Direct2D 'A': $(grep 'DRAW 41' <<<"$out")"
"$WINESERVER" -k 2>/dev/null

if [ "$fails" -eq 0 ]; then echo "RESULT: PASS"; exit 0; fi
echo "RESULT: FAIL ($fails)"; exit 1
