#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Characters beyond the BMP (emoji) in GDI text (patches/sg/0171).
#
# A surrogate pair drew as two missing-glyph boxes, even in a font that has
# the character. The probe draws into a DIB with ExtTextOutW and compares
# pixels; needs Symbola (fonts-symbola) and Liberation Sans (fonts-liberation).
#
#   WINE=/opt/wine-sg/bin/wine test/astral-gate.sh
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
fc-list 2>/dev/null | grep -q 'Symbola' || { echo "SKIP: Symbola is not installed (fonts-symbola)"; exit 77; }
fc-list 2>/dev/null | grep -q 'Liberation Sans' || { echo "SKIP: Liberation Sans is not installed (fonts-liberation)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-astral.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/astral-probe.exe" "$HERE/astral-probe.c" -lgdi32 -lusp10 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/astral-probe.exe" Symbola 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() { if printf '%s\n' "$out" | grep -qx "$1"; then pass "$2"; else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" || echo none))"; fi; }
expect "face=Symbola" "the font with the characters is there"
expect "inked=1" "an emoji draws something"
expect "emoji_differ=1" "different characters beyond the BMP draw differently"
expect "not_box=1" "and not as missing-glyph boxes"
expect "extent_pair=1" "a surrogate pair is one advance wide, nothing for its second half"
expect "extent_mixed=1" "and measures right between other characters"
expect "fallback=1" "a font without emoji falls back to one with them"
expect "fallback_same=1" "the same glyph as from that font"
expect "path=1" "a path gets the character's outline"
expect "uniscribe=1" "Uniscribe's ScriptStringOut (edit controls) falls back too"
expect "ggo_highword=1" "GetGlyphOutlineW still ignores the high word, as on Windows"
if fc-list 2>/dev/null | grep -q 'HanaMinB'; then
    expect "cjkb_fallback=1" "CJK Extension B (U+20000) falls back to HanaMinB (fonts-hanazono, 0220)"
else echo "info  HanaMinB not installed (fonts-hanazono): CJK Extension B not checked"; fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
