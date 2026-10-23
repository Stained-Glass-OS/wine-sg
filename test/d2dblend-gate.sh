#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D blend and composite modes, BlendImage, and effects' invalid and required rectangles
# (patches/sg/6503), under Xvfb (test/d2dblend-probe.cpp):
#  - the Blend effect's blend modes, the Composite effect's modes, the Arithmetic Composite (all
#    were drawn source over before);
#  - ID2D1DeviceContext6::BlendImage (did nothing);
#  - GetEffectRequiredInputRectangles, InvalidateEffectInputRectangle,
#    GetEffectInvalidRectangleCount/Rectangles (E_NOTIMPL): a new or changed effect is all
#    invalid, a drawn one valid, an invalidated input rectangle is carried through the effect and
#    the effects drawn on top of it.
#   WINE=/opt/wine-sg/bin/wine test/d2dblend-gate.sh   (WINESERVER=... beside a build tree)
# Mutants: SG_MUTANT_D2D_BLEND_SOURCE_OVER (effect_render.c), SG_MUTANT_D2D_BLENDIMAGE_NOOP
# (device.c), SG_MUTANT_D2D_REQUIRED_IDENTITY, SG_MUTANT_D2D_INVALID_KEPT (effect_render.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGWXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for need in Xvfb "$MINGWXX"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-d2dblend.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGWXX" -O2 -static -o "$T/d2dblend-probe.exe" "$HERE/d2dblend-probe.cpp" -ld2d1 -ld3d11 -ldxgi \
    -luuid -ldxguid || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/d2dblend-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { echo "SKIP: no Direct2D device here: $(printf '%s\n' "$out" | grep '^no_')"; exit 77; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
for m in multiply screen darken lighten difference exclusion subtract linear_dodge color; do
    check "blend_$m" "Blend effect, mode $m"
done
for m in source_over source_in destination_out xor source_copy; do
    check "composite_$m" "Composite effect, mode $m"
done
check affine_moves           "2D Affine Transform has its properties: a translation moves the image"
check arithmetic_sum         "Arithmetic Composite: destination + source"
check blendimage_inside      "BlendImage multiplies the image into the target"
check blendimage_outside     "and leaves the target alone outside it"
check required_blur          "required input rectangle of a blur: the output grown by 3 sigma"
check required_count_checked "a wrong input count is E_INVALIDARG"
check required_crop          "required input rectangle of a crop: the output inside the crop and the input"
check invalid_new            "a new effect's output is all invalid"
check invalid_drawn          "a drawn effect has no invalid rectangle"
check invalid_input          "an invalidated input rectangle, through the effect"
check invalid_bad_input      "an input the effect does not have is E_INVALIDARG"
check invalid_chain_drawn    "drawing an effect validates the effects under it"
check invalid_chain          "an input invalidated under an effect is carried through it"
check invalid_property       "a changed property makes all of the output invalid"
check done                   "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
