#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Composition effects (patches/sg/9804): ICompositor::CreateEffectFactory and
# CreateEffectFactoryWithProperties read an IGraphicsEffect description through IGraphicsEffectD2D1Interop
# (as Win2D's effects give it) once, CompositionEffectFactory makes CompositionEffectBrushes, their source
# parameters bind brushes by name, animatable properties ("Flood.Color") follow the brush's Properties, and
# the brush draws as its graph of Direct2D effects (a Flood; an Invert of a green colour brush; a Flood
# coloured through Properties). All were E_NOTIMPL. Under Xvfb. test/compeffect-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/compeffect-gate.sh
# Mutants (dlls/dcomp/effects.c): SG_MUTANT_NO_EFFECT_FACTORY (no factory), SG_MUTANT_EFFECT_NOT_ANIMATABLE
# is not reached by Properties (it covers StartAnimation); the Properties colour is caught by
# props_color_drawn when anim_storage ignores the property set.
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
DPY="${COMPEFFECT_DPY:-352}"
case "$DPY" in 0|0.*) echo "FAIL  refusing display :0"; exit 1 ;; esac

T=$(mktemp -d /var/tmp/sg-compeffect.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/compeffect-probe.exe" "$HERE/compeffect-probe.c" \
    -ld3d11 -lole32 -luuid -ldxguid -lruntimeobject -luser32 -lgdi32 \
    || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
unset WAYLAND_DISPLAY XAUTHORITY
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 40 ]; do python3 -c "import time; time.sleep(0.25)"; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 120 "$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/compeffect-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -q "^$1\( \|\$\)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" || echo none))"; fi
}
check 'helpers=1'              "PropertyValue and CompositionEffectSourceParameter factories"
check 'factory=1'              "CreateEffectFactory reads a Flood description"
check 'factory_iface=1'        "the factory is a CompositionEffectFactory and a CompositionObject"
check 'factory_class=1'        "its runtime class is Windows.UI.Composition.CompositionEffectFactory"
check 'factory_status=1'       "LoadStatus Success, ExtendedError S_OK"
check 'brush=1'                "CreateBrush makes a CompositionBrush"
check 'brush_class=1'          "its runtime class is Windows.UI.Composition.CompositionEffectBrush"
check 'factory_param=1'        "an effect over a CompositionEffectSourceParameter"
check 'source_param=1'         "SetSourceParameter / GetSourceParameter by name"
check 'source_param_unknown=1' "a parameter the graph does not have: E_INVALIDARG"
check 'factory_props=1'        "CreateEffectFactoryWithProperties with Flood.Color animatable"
check 'props_insert=1'         "the brush's Properties take Flood.Color"
check 'bad_animatable=1'       "a property the effect does not map is refused"
check 'no_interop=1'           "an effect without IGraphicsEffectD2D1Interop is refused"
check 'null_effect=1'          "no effect is refused"
if printf '%s\n' "$out" | grep -q '^no_d3d11='; then
    echo "SKIP  drawing: no Direct3D 11 device on this display"
else
    check 'flood_drawn=1'      "the Flood brush draws its colour (as described when the factory was made)"
    check 'invert_drawn=1'     "the Invert brush draws its green source inverted"
    check 'props_color_drawn=1' "the animatable colour comes from the brush's Properties"
    check 'outside_white=1'    "nothing is drawn outside the sprites"
fi
check 'done=1'                 "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
