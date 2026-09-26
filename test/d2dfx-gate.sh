#!/bin/sh
# Direct2D, DXGI, the shader compiler and Windows Animation as Paint.NET
# uses them (patches/sg/0223-0228), under Xvfb:
#
#  - Direct2D's built-in effects are registered with their properties'
#    types and defaults, and those can be set;
#  - a custom effect made by a .NET ComWrappers-style factory is initialized
#    through its ID2D1EffectImpl; its context is an ID2D1EffectContext2 that
#    reports the feature level and makes transform nodes of effects;
#  - a WIC bitmap render target's device has feature level 11; Flush says
#    whether the context is drawing; an empty command list closes and one
#    set as the target mid-frame records; sRGB color contexts carry an ICC
#    profile; gradients are ID2D1GradientStopCollection1; geometry
#    realizations exist and draw (0223);
#  - DXGI has a WARP adapter (0224); shader reflection reports a shader's
#    minimum feature level (0225);
#  - Windows Animation moves a variable along a storyboard, and its timer
#    ticks (0226);
#  - an effect's bounds come from its graph (a blur of a bitmap grows by
#    three deviations); CombineWithGeometry and Widen give the right areas
#    (0227); DXGI outputs answer CheckHardwareCompositionSupport (0228).
#
#   WINE=/opt/wine-sg/bin/wine test/d2dfx-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${D2DFX_DPY:-176}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-d2dfx.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/d2dfx-probe.exe" "$HERE/d2dfx-probe.c" \
    -ld2d1 -ld3d11 -ld3dcompiler_47 -lole32 -luuid -ldxguid || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/d2dfx-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -qx "$1"; }
check() { if has "$1"; then pass "$2"; else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" || echo none))"; fi; }

printf '%s\n' "$out" | grep -q '^no_d3d11=' && { echo "SKIP: no Direct3D 11 device here"; exit 77; }
check 'builtin_props=12/12'           "the built-in effects Paint.NET asks for are registered"
check 'builtin_create=12/12'          "and can be created"
check 'builtin_typed=1'               "their properties have their types and defaults (a blur's deviation, a color context)"
check 'builtin_settable=1'            "and a built-in effect's properties can be set"
check 'custom_created=1'              "a custom effect from a ComWrappers-style factory is created"
check 'initialized_through_impl=1'    "through its ID2D1EffectImpl, not its IUnknown"
check 'context2=1'                    "its context is an ID2D1EffectContext2"
check 'max_level_ok=1'                "which reports the feature level"
check 'effect_node=1'                 "and makes a transform node of an effect"
check 'getter_level=0xa000'           "a bound property's getter answers"
check 'wic_target_level11=1'          "a WIC bitmap render target's effects get feature level 11"
check 'flush_states=1'                "Flush says whether the context is drawing"
check 'cmdlist_close_empty=1'         "an empty command list closes"
check 'cmdlist_record_midframe=1'     "a command list set as the target mid-frame records"
check 'srgb_profile=1'                "an sRGB color context carries an ICC profile"
check 'gradient1=1'                   "gradient stop collections are ID2D1GradientStopCollection1"
check 'realization=1'                 "geometry realizations are made and drawn"
check 'warp_adapter=1'                "DXGI has a WARP adapter"
if printf '%s\n' "$out" | grep -qx 'hw_composition=no-output'; then
    echo "SKIP  the adapter has no output here"
else
    check 'hw_composition=1'          "its output answers CheckHardwareCompositionSupport (none)"
fi
if printf '%s\n' "$out" | grep -q '^min_feature_level=0xb000,'; then
    pass "shader reflection: a shader model 5 shader needs feature level 11_0"
else
    fail "shader reflection: $(printf '%s\n' "$out" | grep '^min_feature_level=' || echo none)"
fi
if printf '%s\n' "$out" | grep -q '^animation=1 '; then
    pass "Windows Animation moves a variable along a storyboard, and finishes"
else
    fail "animation: $(printf '%s\n' "$out" | grep '^animation=' || echo none)"
fi
check 'animation_timer=1'             "its timer tells the time and can be enabled"
if printf '%s\n' "$out" | grep -q '^effect_bounds=1 '; then
    pass "an effect's bounds: a blur of a 64x64 bitmap by 3 covers -9..73"
else
    fail "effect bounds: $(printf '%s\n' "$out" | grep '^effect_bounds=' || echo none)"
fi
check 'combine=4/4'                   "CombineWithGeometry: union, intersection, xor and difference of two squares"
check 'widen_area=80.0'               "Widen: a square's 2-wide mitered stroke covers 12x12 - 8x8"
check 'done=1'                        "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
