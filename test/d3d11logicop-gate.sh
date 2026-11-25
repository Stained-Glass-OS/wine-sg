#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct3D 11.1 output-merger logic ops (patches/sg/9305), wined3d (no DXVK), under Xvfb:
# test/d3d11logicop-probe.cpp. wined3d reports feature level 11_1, where Direct3D has every logic op, but
# ignored the blend state's LogicOp ("Ignoring logic op") and wrote the shader's colour as COPY does;
# CheckFeatureSupport(D3D11_OPTIONS) said "fake" data with OutputMergerLogicOp FALSE. Now each of the 16
# D3D11_LOGIC_OPs combines the shader's value with the render target's (OpenGL glLogicOp, Vulkan logicOp),
# a logic op with IndependentBlendEnable is refused (E_INVALIDARG, as Direct3D does), OPTIONS reports the
# logic op and ClearView that are implemented, and the default blend state writes plainly again.
#   WINE=/opt/wine-sg/bin/wine test/d3d11logicop-gate.sh
#   D3D11LOGICOP_RENDERER=vulkan for wined3d's Vulkan renderer
# Mutant: SG_MUTANT_NO_LOGIC_OP (dlls/wined3d/ffp_gl.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGWXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGWXX" >/dev/null || { echo "SKIP: $MINGWXX missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-d3d11logicop.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGWXX" -O2 -static -o "$T/probe.exe" "$HERE/d3d11logicop-probe.cpp" -ld3d11 -ldxgi \
    -ld3dcompiler_47 -luuid -ldxguid || { fail "probe did not build"; exit 1; }
sg_gate_display || exit $?
unset WAYLAND_DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
[ -n "${D3D11LOGICOP_RENDERER:-}" ] && export WINE_D3D_CONFIG="renderer=$D3D11LOGICOP_RENDERER"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { echo "SKIP: no Direct3D 11 here: $(printf '%s\n' "$out" | grep '^no_')"; exit 77; }
printf '%s\n' "$out" | grep -q '^feature_level=0xb100' || { echo "SKIP: not feature level 11_1 here"; exit 77; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check options_logicop    "CheckFeatureSupport(D3D11_OPTIONS) reports OutputMergerLogicOp"
check options_clearview  "and ClearView, which is implemented"
check rejects_independent "a logic op with IndependentBlendEnable is refused (E_INVALIDARG)"
check all_ops            "all 16 logic ops combine the shader's value with the render target's"
check default_state      "the default blend state writes the shader's value again"
check done               "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
