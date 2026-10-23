#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The PostScript driver with OpenType CFF fonts and exclusive clipping (patches/sg/6507): text in a
# CFF font printed as nothing (wineps found no 'glyf' outline and downloaded empty glyphs), and
# CLIP_TO_PATH(CLIP_EXCLUSIVE) was not implemented. test/pscff-probe.c prints through wineps to a
# file; ghostscript renders it.
#   WINE=/opt/wine-sg/bin/wine test/pscff-gate.sh
# Mutants: SG_MUTANT_PS_CFF_EMPTY (type1.c), SG_MUTANT_PS_CLIP_EXCLUSIVE_NONE (escape.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in gs convert "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pscff.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -O1 -o "$T/probe.exe" "$HERE/pscff-probe.c" -lwinspool -lgdi32 || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
export CUPS_SERVER="$T/no-cups.sock"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
"$WINESERVER" -w
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_cff_font=1' && { echo "SKIP: no OpenType CFF font on this system"; exit 77; }
printf '%s\n' "$out" | grep -q '^no_' && { fail "the printer could not be set up"; exit 1; }
PS="$WINEPREFIX/drive_c/out.ps"
[ -s "$PS" ] && pass "the job was written" || { fail "no PostScript"; exit 1; }
gs -q -dNOPAUSE -dBATCH -dSAFER -sDEVICE=pnggray -r50 -sOutputFile="$T/page%d.png" "$PS" >"$T/gs.log" 2>&1
grep -qi error "$T/gs.log" && fail "ghostscript reported errors: $(head -3 "$T/gs.log")" || pass "ghostscript reads the job"
ink() { convert "$T/$1" -crop "$2" -negate -threshold 50% -format "%[fx:mean*w*h]" info: 2>/dev/null; }
px() { convert "$T/$1" -crop "1x1+$2+$3" -format "%[fx:int(255*p{0,0}.r)]" info: 2>/dev/null; }
# 300 dpi printer units at 50 dpi: / 6
text=$(ink page1.png 200x40+45+45)
[ "${text:-0}" -gt 50 ] 2>/dev/null && pass "text in a CFF font is printed ($text dark pixels)" || fail "text in a CFF font is missing (${text:-none} dark pixels)"
hole=$(px page2.png 212 212); fill=$(px page2.png 80 80)
[ "$fill" = 0 ] && [ "$hole" = 255 ] && pass "CLIP_EXCLUSIVE: the fill is everywhere but inside the path" ||
    fail "CLIP_EXCLUSIVE: inside the path $hole, outside $fill (want 255, 0)"
printf '%s\n' "$out" | grep -Eq '^clip_exclusive_ret=[1-9]' && pass "CLIP_TO_PATH(CLIP_EXCLUSIVE) succeeds" || fail "CLIP_EXCLUSIVE failed"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
