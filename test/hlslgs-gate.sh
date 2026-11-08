#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# HLSL geometry shaders compile (wine-sg 6273: libs/vkd3d is upstream vkd3d 2.1, was 1.14). RetroArch's
# first start picks its d3d11 driver, whose menu draws sprites with a geometry shader (point VSOut
# in[1], inout TriangleStream<GSOut>); vkd3d 1.14's HLSL compiler did not parse it, D3DCompile failed
# and RetroArch exited. test/hlslgs-probe.c: compiled for gs_4_0 and gs_5_0, reflection gives its
# maxvertexcount, a Direct3D 11 device takes it. (Wine's reflection does not give the input primitive and
# output topology of a vkd3d-compiled shader: printed, not checked.)
#   WINE=/opt/wine-sg/bin/wine test/hlslgs-gate.sh
# Mutant: SG_MUTANT_HLSL_NO_POINT (defined in libs/vkd3d/libs/vkd3d-shader/hlsl.h: the point modifier is
# refused again; vkd3d is linked into wined3d.dll, which must be relinked: remove it before make).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-hlslgs.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/hlslgs-probe.c" -ld3d11 -ld3dcompiler_47 -luuid || { fail "probe did not build"; exit 1; }
if [ -n "${SG_HLSL_DISPLAY:-}" ]; then
    Xvfb ":$SG_HLSL_DISPLAY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -e "/tmp/.X11-unix/X$SG_HLSL_DISPLAY" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    echo "$SG_HLSL_DISPLAY" > "$T/display"
else
    Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 300 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check compile_gs_4_0 "a point-in, triangle-strip-out geometry shader (RetroArch's menu sprites) compiles for gs_4_0"
check compile_gs_5_0 "and for gs_5_0"
check reflect_gs_4_0 "reflection: at most 4 vertices out (gs_4_0)"
check reflect_gs_5_0 "and for gs_5_0"
if printf '%s\n' "$out" | grep -q '^no_device=1'; then echo "      (no Direct3D 11 device here: CreateGeometryShader not tried)"
else
    check device_gs_4_0 "a Direct3D 11 device takes the gs_4_0 shader"
    check device_gs_5_0 "and the gs_5_0 one"
fi
check done            "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
