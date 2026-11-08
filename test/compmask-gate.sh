#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Composition for Teams' title bar overlay (wine-sg 6270): ICompositor2::CreateMaskBrush makes a
# CompositionMaskBrush (it was E_NOTIMPL, and Teams threw on it about every second start); its Source is
# drawn through its Mask's alpha -- a colour mask at half alpha gives half the source, a surface mask's
# picture shows the source where it is opaque and nothing where it is clear. CompositionCapabilities is
# registered (RoGetActivationFactory found no library): GetForCurrentView, and it says what our renderer
# does -- no effect brushes, so effects are neither supported nor fast -- and Changed takes a handler.
# test/compmask-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/compmask-gate.sh
# 6272: CompositionEffectSourceParameter (an effect's named source) is made, and a window target
# supports a system backdrop (Teams asks it for ICompositionSupportsSystemBackdrop): a brush of ours
# set as the backdrop fills the client area under the root.
# Mutants (dlls/dcomp): SG_MUTANT_NO_MASK_BRUSH, SG_MUTANT_MASK_IGNORED (composition.c),
# SG_MUTANT_NO_CAPABILITIES (composition.c), SG_MUTANT_EFFECTS_CLAIMED (capabilities.c);
# 6272: SG_MUTANT_NO_SOURCE_PARAMETER (capabilities.c), SG_MUTANT_NO_SYSTEM_BACKDROP (composition.c).
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
DPY=$(( 400 + $$ % 500 ))
while [ -e "/tmp/.X$DPY-lock" ] || [ -e "/tmp/.X11-unix/X$DPY" ]; do DPY=$((DPY + 1)); done

T=$(mktemp -d /var/tmp/sg-compmask.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -rf "$T" "${SG_GATE_HOME:-$T}"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/compmask-probe.exe" "$HERE/compmask-probe.c" \
    -ld2d1 -ld3d11 -lole32 -luuid -ldxguid -lruntimeobject -luser32 -lgdi32 \
    || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/compmask-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -q "^$1\( \|\$\)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" || echo none))"; fi
}
check 'caps_factory=1'      "CompositionCapabilities has an activation factory (its statics)"
check 'caps_for_view=1'     "GetForCurrentView gives the capabilities"
check 'caps_effects=0,0'    "effects are neither supported nor fast (the renderer makes no effect brushes)"
check 'caps_changed=1'      "Changed takes a handler and gives it back"
check 'param_factory=1'     "CompositionEffectSourceParameter has its factory (6272)"
check 'param_made=1'        "it makes a source parameter with a name"
check 'param_name=1'        "which keeps its name"
check 'param_effect_source=1' "and is an IGraphicsEffectSource"
if printf '%s\n' "$out" | grep -q '^no_d3d11='; then
    echo "SKIP: no Direct3D 11 device here for the drawing part"
else
    check 'mask_brush=1'          "ICompositor2::CreateMaskBrush makes a CompositionMaskBrush"
    check 'mask_class=1'          "its runtime class is Windows.UI.Composition.CompositionMaskBrush"
    check 'mask_roundtrip=1'      "its Mask is kept"
    check 'surface_mask=1'        "a surface brush can be its mask"
    check 'color_mask=1'          "a colour mask at half alpha: half the red source on white"
    check 'surface_mask_opaque=1' "a surface mask: the source where the picture is opaque"
    check 'surface_mask_clear=1'  "and nothing where it is clear"
    check 'backdrop_iface=1'      "a window target supports a system backdrop (ICompositionSupportsSystemBackdrop, 6272)"
    check 'backdrop_roundtrip=1'  "its SystemBackdrop is kept"
    check 'backdrop_drawn=1'      "a colour brush as the backdrop fills the window under the root"
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
