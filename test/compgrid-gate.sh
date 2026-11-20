#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ICompositor2::CreateDropShadow (and ISpriteVisual2.Shadow), CreateNineGridBrush
# (drawn) and CreateBackdropBrush (patches/sg/8215): test/compgrid-probe.c under
# Xvfb. They were E_NOTIMPL.
#
#   WINE=/opt/wine-sg/bin/wine test/compgrid-gate.sh
# Mutant: SG_MUTANT_NO_COMPOSITOR2_OBJECTS (dcomp/composition.c).
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
if printf '%s\n' "$out" | grep -q '^no_d3d11='; then
    echo "SKIP: no Direct3D 11 device here for the drawing part"
else
    check 'nine_grid_edge=1'       "a nine-grid brush draws its source's edges"
    check 'nine_grid_hollow=1'     "and leaves a hollow centre out"
    check 'backdrop_shows_behind=1' "a backdrop brush shows what is behind it"
fi
check 'done=1'                 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
