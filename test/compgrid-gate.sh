#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ICompositor2::CreateDropShadow (and ISpriteVisual2.Shadow), CreateNineGridBrush
# (drawn), CreateBackdropBrush (patches/sg/8215) and CreateAnimationGroup with
# StartAnimationGroup and implicit groups (8216), CreateLayerVisual (8218) and
# the four lights (8219): test/compgrid-probe.c under Xvfb. They were E_NOTIMPL.
#
#   WINE=/opt/wine-sg/bin/wine test/compgrid-gate.sh
# Mutants: SG_MUTANT_NO_COMPOSITOR2_OBJECTS (dcomp/composition.c),
# SG_MUTANT_NO_ANIMATION_GROUP (dcomp/composition.c and animation.c),
# SG_MUTANT_NO_LAYER_VISUAL (dcomp/composition.c),
# SG_MUTANT_NO_LIGHTS (dcomp/composition.c and lights.c).
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
DPY="${COMPGRID_DPY:-$(( 400 + $$ % 500 ))}"
while [ -e "/tmp/.X$DPY-lock" ] || [ -e "/tmp/.X11-unix/X$DPY" ]; do DPY=$((DPY + 1)); done

T=$(mktemp -d /var/tmp/sg-compgrid.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -rf "$T" "${SG_GATE_HOME:-$T}"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/compgrid-probe.exe" "$HERE/compgrid-probe.c" \
    -ld2d1 -ld3d11 -lole32 -luuid -ldxguid -lruntimeobject -luser32 -lgdi32 \
    || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/compgrid-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -q "^$1\( \|\$\)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" || echo none))"; fi
}
check 'drop_shadow=1'          "ICompositor2::CreateDropShadow makes a DropShadow"
check 'drop_shadow_class=1'    "its runtime class is Windows.UI.Composition.DropShadow"
check 'drop_shadow_is_shadow=1' "it is a CompositionShadow"
check 'drop_shadow_defaults=1' "blur radius 9, opaque, black at first"
check 'drop_shadow_radius=1'   "its BlurRadius is kept"
check 'sprite2=1'              "a sprite visual has ISpriteVisual2"
check 'sprite_shadow=1'        "and keeps a Shadow"
check 'nine_grid=1'            "ICompositor2::CreateNineGridBrush makes a CompositionNineGridBrush"
check 'nine_grid_class=1'      "its runtime class is Windows.UI.Composition.CompositionNineGridBrush"
check 'nine_grid_props=1'      "its insets, hollow centre and source are kept"
check 'backdrop_brush=1'       "ICompositor2::CreateBackdropBrush makes a CompositionBackdropBrush"
check 'backdrop_class=1'       "its runtime class is Windows.UI.Composition.CompositionBackdropBrush"
check 'layer=1'                "ICompositor2::CreateLayerVisual makes a LayerVisual (8218)"
check 'layer_class=1'          "its runtime class is Windows.UI.Composition.LayerVisual"
check 'layer_container=1'      "it is a container visual"
check 'layer_effect=1'         "its Effect is kept"
check 'layer_shadow=1'         "its Shadow is kept (ILayerVisual2)"
check 'lights_made=1'          "ICompositor2 makes ambient, distant, point and spot lights (8219)"
check 'lights_class=1'         "with their runtime classes"
check 'lights_props=1'         "white, constant attenuation 1, cone angle in degrees and radians, enabled"
check 'light_targets=1'        "a light's Targets keep a visual once and iterate"
check 'group=1'                "ICompositor2::CreateAnimationGroup makes a CompositionAnimationGroup"
check 'group_class=1'          "its runtime class is Windows.UI.Composition.CompositionAnimationGroup"
check 'group_items=1'          "Add, Remove, Count and iteration"
check 'group_started=1'        "StartAnimationGroup runs each animation on its Target"
check 'group_needs_targets=1'  "an animation without a Target makes StartAnimationGroup fail"
check 'group_implicit=1'       "a group as an implicit animation runs its animations on their Targets"
if printf '%s\n' "$out" | grep -q '^no_d3d11='; then
    echo "SKIP: no Direct3D 11 device here for the drawing part"
else
    check 'nine_grid_edge=1'       "a nine-grid brush draws its source's edges"
    check 'nine_grid_hollow=1'     "and leaves a hollow centre out"
    check 'backdrop_shows_behind=1' "a backdrop brush shows what is behind it"
    check 'layer_children_drawn=1' "a layer visual's children are drawn"
fi
check 'done=1'                 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
