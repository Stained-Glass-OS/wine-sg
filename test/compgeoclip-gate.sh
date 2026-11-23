#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Composition geometric clips (patches/sg/9806): ICompositor6::CreateGeometricClip and
# CreateGeometricClipWithGeometry make CompositionGeometricClips (Geometry and ViewBox kept), and a visual
# clipped by one draws only inside the geometry (a Direct2D layer with the geometry as mask). They were
# E_NOTIMPL. And ICompositor6::CreateRedirectVisual(WithSourceVisual): a RedirectVisual (a container) keeps
# its Source and draws that visual's tree again where it is. Under Xvfb. test/compgeoclip-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/compgeoclip-gate.sh
# Mutants (dlls/dcomp/composition.c): SG_MUTANT_NO_GEOMETRIC_CLIP, SG_MUTANT_GEOMETRIC_CLIP_NOT_DRAWN,
# SG_MUTANT_NO_REDIRECT_VISUAL, SG_MUTANT_REDIRECT_NOT_DRAWN.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
DPY="${COMPGEOCLIP_DPY:-355}"
case "$DPY" in 0|0.*) echo "FAIL  refusing display :0"; exit 1 ;; esac

T=$(mktemp -d /var/tmp/sg-compgeoclip.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/compgeoclip-probe.exe" "$HERE/compgeoclip-probe.c" \
    -ld3d11 -lole32 -luuid -ldxguid -lruntimeobject -luser32 -lgdi32 \
    || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
unset WAYLAND_DISPLAY XAUTHORITY
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 40 ]; do python3 -c "import time; time.sleep(0.25)"; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 120 "$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/compgeoclip-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -q "^$1\( \|\$\)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" || echo none))"; fi
}
check 'compositor6=1'     "the compositor has ICompositor5 and ICompositor6"
check 'clip=1'            "CreateGeometricClip makes a CompositionGeometricClip, a CompositionClip"
check 'clip_class=1'      "its runtime class is Windows.UI.Composition.CompositionGeometricClip"
check 'clip_props=1'      "its Geometry is kept (and ViewBox taken)"
check 'clip_with=1'       "CreateGeometricClipWithGeometry keeps its geometry"
check 'redirect=1'        "CreateRedirectVisual makes a RedirectVisual, a container visual"
check 'redirect_class=1'  "its runtime class is Windows.UI.Composition.RedirectVisual"
if printf '%s\n' "$out" | grep -q '^no_d3d11='; then
    echo "SKIP  drawing: no Direct3D 11 device on this display"
else
    check 'inside_drawn=1'    "the clipped sprite draws inside the geometry"
    check 'outside_clipped=1' "and nothing outside it"
    check 'redirect_source=1' "a RedirectVisual keeps its Source"
    check 'redirect_with=1'   "CreateRedirectVisualWithSourceVisual keeps its source"
    check 'redirect_drawn=1'  "the RedirectVisual draws its Source (clip and all) where it is"
fi
check 'done=1'            "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
