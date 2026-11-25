#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# EMF+ texture and path gradient brushes (patches/sg/9201, 9203): recording a fill or a pen with a
# TextureBrush or PathGradientBrush into an EMF+ metafile failed with NotImplemented (System.Drawing
# programs drawing into a Metafile got "not implemented"), and a path gradient brush object in a
# metafile ([MS-EMFPLUS] 2.2.2.29) was not read, so its fill was dropped. As on Windows, both are
# written and read, and a played metafile draws what drawing the brush directly draws. The probe
# (test/emfpbrush-probe.c) compares direct and recorded+played pictures and plays a hand-written
# point-boundary path gradient record. 9203: a path gradient's transform and focus scales were ignored
# when drawn ("not implemented"); now drawn as Windows draws them.
#   WINE=/opt/wine-sg/bin/wine test/emfpbrush-gate.sh
# Mutants: SG_MUTANT_EMFP_NO_TEXTURE_PATHGRADIENT (metafile.c), SG_MUTANT_PATHGRAD_NO_TRANSFORM_FOCUS (graphics.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-emfpbrush.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/emfpbrush-probe.c" -lgdi32 || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { fail "probe could not start"; exit 1; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check focus_inner         "a path gradient's focus scales fill the scaled boundary with the centre colour"
check focus_mid           "and the gradient runs from the boundary to the focus"
check transform_center    "a path gradient's transform moves its centre"
check transform_edge      "and scales its boundary"
check texture_recorded    "a fill with a texture brush is recorded"
check texture_object      "its brush object is a texture holding an EMF+ image"
check texture_same        "played, it draws what the brush draws directly"
check pathgrad_recorded   "a fill with a path gradient brush (focus scales) is recorded"
check pathgrad_object     "its brush object is a path gradient with a boundary path and focus scales"
check pathgrad_same       "played, it draws what the brush draws directly"
check preset_recorded     "a path gradient with preset colours is recorded"
check preset_same         "played, its preset colours are drawn"
check texpen_recorded     "a pen with a texture brush is recorded"
check texpen_same         "played, the textured line is drawn"
check handwritten_object  "a hand-written path gradient object (point boundary, transform) is read"
check handwritten_center  "its centre colour is at the transformed centre point"
check handwritten_edge    "its surround colour is at the boundary"
check done                "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
