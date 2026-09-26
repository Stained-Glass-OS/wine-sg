#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D layers, primitive blends and DrawImage's composite modes
# (patches/sg/0340), under Xvfb: test/d2dlayer-probe.c draws on a WIC bitmap
# render target and reads the pixels back.
#
#  - PushLayer/PopLayer: opacity, content bounds, a geometric mask, an
#    opacity brush, nested layers, drawing on the target again after the
#    pop, and the device context's PushLayer (were stubs: the content was
#    drawn straight on the target, unclipped and opaque);
#  - SetPrimitiveBlend: COPY, ADD, MIN (were drawn source-over);
#  - DrawImage's composite modes: DESTINATION_OVER, DESTINATION_OUT, XOR
#    (were drawn source-over), and the next drawing is source-over again.
#
#   WINE=/opt/wine-sg/bin/wine test/d2dlayer-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${D2DLAYER_DPY:-177}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-d2dlayer.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/d2dlayer-probe.exe" "$HERE/d2dlayer-probe.c" \
    -ld2d1 -lwindowscodecs -lole32 -luuid -ldxguid || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/d2dlayer-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
printf '%s\n' "$out" | grep -q '^no_' && { fail "no Direct2D target: $out"; exit 1; }
check layer_opacity              "a layer at half opacity: red drawn in it is pink on white"
check layer_bounds_inside        "a layer's content is drawn inside its content bounds"
check layer_bounds_outside       "and not outside them"
check layer_mask_inside          "a layer's content is drawn inside its geometric mask (a circle)"
check layer_mask_outside         "and not outside it (the corners)"
check layer_opacity_brush        "a transparent opacity brush hides the layer"
check layer_nested               "nested layers at half opacity each: a quarter"
check layer_popped               "after the pop, drawing goes to the target"
check layer1_inside              "the device context's PushLayer: inside its bounds"
check layer1_outside             "and not outside"
check blend_copy                 "primitive blend COPY replaces the pixel, alpha included"
check blend_add                  "primitive blend ADD adds"
check blend_min                  "primitive blend MIN keeps the smaller"
check composite_dest_over_kept   "DrawImage DESTINATION_OVER: the image goes behind what is opaque"
check composite_dest_over_behind "and shows where nothing was"
check composite_dest_out         "DrawImage DESTINATION_OUT cuts out what the image covers"
check composite_xor              "DrawImage XOR of two opaque pixels leaves nothing"
check composite_reset            "after DrawImage, drawing is source-over again"
check done                       "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
