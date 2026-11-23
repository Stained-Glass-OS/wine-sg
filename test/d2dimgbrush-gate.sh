#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D image brushes of any image and BlendImage in command lists (patches/sg/9504), under Xvfb
# (test/d2dimgbrush-probe.cpp):
#  - an image brush paints the source rectangle of an effect's output (a flood), of a command list (tiled
#    with WRAP) and of part of a bitmap (CLAMP extends the part's edge); before, only whole bitmaps were
#    painted ("Only image brushes with bitmaps are supported") and the rest drew nothing;
#  - a bitmap brush with a rotated transform maps a non-square bitmap (the scale was applied after the
#    rotation);
#  - BlendImage into a command list is recorded and blends when the list is drawn (was dropped).
#   WINE=/opt/wine-sg/bin/wine test/d2dimgbrush-gate.sh   (WINESERVER=... beside a build tree)
# Mutants: SG_MUTANT_D2D_IMAGE_BRUSH_BITMAPS_ONLY (brush.c), SG_MUTANT_D2D_BLEND_NOT_RECORDED (device.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGWXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for need in Xvfb xdpyinfo "$MINGWXX"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-d2dimgbrush.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGWXX" -O2 -static -o "$T/d2dimgbrush-probe.exe" "$HERE/d2dimgbrush-probe.cpp" -ld2d1 -ld3d11 -ldxgi \
    -luuid -ldxguid || { fail "probe did not build"; exit 1; }
DPY="${D2DIMGBRUSH_DPY:-324}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
unset WAYLAND_DISPLAY
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/d2dimgbrush-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { echo "SKIP: no Direct2D device here: $(printf '%s\n' "$out" | grep '^no_')"; exit 77; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check effect_brush            "an effect's output (a green flood) as an image brush, tiled"
check list_brush_red          "a command list as an image brush: its red part"
check list_brush_blue         "its blue part"
check list_brush_wrap_x       "wrapped across"
check list_brush_wrap_y       "wrapped down"
check bitmap_part_inside      "part of a bitmap (the blue half) as an image brush"
check bitmap_part_clamped     "clamped beyond the part: its edge, not the rest of the bitmap"
check bitmap_whole_left       "the whole bitmap as before: left half"
check bitmap_whole_right      "right half"
check rotated_bitmap_red      "a 16 x 4 bitmap brush turned 90 degrees: red where its left half lands"
check rotated_bitmap_blue     "blue where its right half lands"
check list_blendimage_inside  "BlendImage recorded in a command list multiplies when the list is drawn"
check list_blendimage_outside "and leaves the target alone outside the image rectangle"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
