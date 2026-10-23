#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# gdi32 engine services for user-mode printer drivers (6062, C2): path, line, fill, blend and
# gradient services answered "not supported" (FIXME, 0). A driver drawing on its own 32-bit surface:
#  - CLIPOBJ_ppoGetPath gives a rectangle clip's outline (none for a trivial clip); PATHOBJ_vEnumStart /
#    bEnum enumerate it (one closed subpath, the four corners), PATHOBJ_vGetBounds bounds it; EngDeletePath;
#  - EngFillPath fills exactly the pixels inside (alternate and winding), clipped by another clip, with the
#    mix (XOR twice restores); a pattern brush fails;
#  - EngStrokePath draws the cosmetic outline (each segment's last pixel left out), refuses geometric lines;
#    EngStrokeAndFillPath fills then strokes;
#  - EngLineTo draws a Bresenham line, clipped; EngPaint fills a clip's area (with R2_NOT too);
#  - EngAlphaBlend with constant alpha and with a premultiplied source; EngGradientFill rectangles and
#    triangles (a bad mode fails);
#  - BRUSHOBJ_pvAllocRbrush / pvGetRbrush; BRUSHOBJ_hGetColorTransform has no transform.
#   WINE=/opt/wine-sg/bin/wine test/engpaths-gate.sh   (mutants SG_MUTANT_PATH_ENUM_MORE,
#   SG_MUTANT_ALPHA_IGNORE_CONSTANT, SG_MUTANT_GRADIENT_FLAT; the winding rule cannot be told from the
#   alternate one through the API, a driver's only path being a clip's rectangle)
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
T=$(mktemp -d /var/tmp/sg-engpaths.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/engpaths-probe.exe" "$HERE/engpaths-probe.c" || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/engpaths-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" engpaths-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect TrivialPath 1 "a trivial clip has no outline"
expect Path "1 4" "CLIPOBJ_ppoGetPath gives a rectangle clip's outline"
expect Enum "0 4 0xb 4,6 12,6 12,10 4,10" "PATHOBJ_bEnum: one closed subpath of the four corners, the last"
expect EnumEnd "0 0" "and nothing after it"
expect Bounds "4 6 12 10" "PATHOBJ_vGetBounds"
expect Fill "1 32 1 1 1" "EngFillPath fills the 8x4 pixels inside and nothing outside"
expect FillClipped "1 8" "clipped by another clip (winding)"
expect FillPattern 0 "a pattern brush needs DrvRealizeBrush: it fails"
expect FillXor "32 1024" "with R2_XORPEN, twice gives back the picture"
expect Stroke "1 24 1 1" "EngStrokePath draws the outline's 24 pixels"
expect StrokeWide 0 "and refuses a geometric line"
expect StrokeAndFill "1 1 1" "EngStrokeAndFillPath fills, then strokes"
expect Line "1 10 1 1" "EngLineTo: a Bresenham line without its last pixel"
expect LineClipped "1 8" "clipped"
expect Paint "1 32" "EngPaint fills the clip's area"
expect PaintNot "1 32" "with R2_NOT it inverts"
expect AlphaConstant "1 0x7f7fff 0xffffff" "EngAlphaBlend with constant alpha, only in the destination rectangle"
expect AlphaSource "1 0x7f7fff" "and with a premultiplied source"
expect GradientH "1 0 0x7f0000 0xef0000" "EngGradientFill: a horizontal rectangle"
expect GradientTriangle "1 1 1" "a triangle, nothing outside it"
expect GradientBadMode 0 "a bad mode fails"
expect Rbrush "1 1 1" "BRUSHOBJ_pvAllocRbrush, pvGetRbrush (none for an unrealized brush)"
expect RbrushAgain "1 1" "a second allocation replaces the first"
expect ColorTransform 1 "BRUSHOBJ_hGetColorTransform: no transform"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
