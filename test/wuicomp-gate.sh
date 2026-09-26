#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Composition's first slice and what Paint.NET needs around it
# (patches/sg/0229-0234), under Xvfb:
#
#  - Windows.UI.Composition.Compositor activates in a fresh prefix (dcomp.dll
#    registers its classes); a desktop window target made through
#    ICompositorDesktopInterop shows a container visual holding a sprite
#    visual with a red color brush and one with a surface brush, the surface
#    a composition drawing surface cleared green with Direct2D through
#    ICompositionDrawingSurfaceInterop. The window's pixels are read back:
#    red where the first sprite is, green where the second is, neither
#    outside them (0231);
#  - Direct3D 11 devices are ID3D11Device5, immediate contexts
#    ID3D11DeviceContext4, DXGI devices IDXGIDevice4 (0232);
#  - WIC converts 32bppBGRA to 64bppPRGBAHalf with the right linear values,
#    and Direct2D makes a bitmap of that format (0230, 0229);
#  - SetWindowFeedbackSetting is kept and GetWindowFeedbackSetting reads it
#    back (0233).
#
# Direct2D's effect drawing, command lists and geometries (0229) are in
# test/d2dfx-gate.sh.
#
#   WINE=/opt/wine-sg/bin/wine test/wuicomp-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${WUICOMP_DPY:-177}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wuicomp.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/wuicomp-probe.exe" "$HERE/wuicomp-probe.c" \
    -ld2d1 -ld3d11 -lole32 -luuid -ldxguid -lwindowscodecs -lruntimeobject -luser32 -lgdi32 \
    || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/wuicomp-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -qx "$1"; }
check() {
    if printf '%s\n' "$out" | grep -q "^$1\( \|\$\)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" || echo none))"; fi
}

printf '%s\n' "$out" | grep -q '^no_d3d11=' && { echo "SKIP: no Direct3D 11 device here"; exit 77; }
check 'd3d11_device5=1'         "Direct3D 11 devices are ID3D11Device5"
check 'd3d11_context4=1'        "their immediate contexts are ID3D11DeviceContext4"
check 'dxgi_device4=1'          "DXGI devices are IDXGIDevice4"
check 'wic_half=1'              "WIC converts 32bppBGRA to 64bppPRGBAHalf, linear"
check 'd2d_from_wic_half=1'     "Direct2D makes a bitmap from a 64bppPRGBAHalf WIC bitmap"
check 'compositor_activated=1'  "Windows.UI.Composition.Compositor activates in a fresh prefix"
check 'compositor_interfaces=1' "it is ICompositor, ICompositorInterop and ICompositorDesktopInterop"
check 'desktop_target=1'        "a desktop window target is made for a window"
check 'drawing_surface=1'       "a graphics device from a Direct2D device makes a drawing surface"
check 'surface_begin_draw=1'    "BeginDraw hands out a Direct2D device context"
check 'surface_end_draw=1'      "and EndDraw ends it"
check 'window_left=1'           "the window shows the red color-brush sprite"
check 'window_right=1'          "and the green drawing-surface sprite"
check 'window_uncovered=1'      "and neither outside them"
check 'feedback_set=1'          "SetWindowFeedbackSetting succeeds"
check 'feedback_get=1'          "GetWindowFeedbackSetting reads the setting back"
check 'done=1'                  "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
