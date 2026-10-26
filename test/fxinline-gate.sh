#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Inline shaders in effects (patches/sg/6543). A pass that names its shaders with
# SetVertexShader(CompileShader(vs_4_0, VS())) did not compile with fx_5_0 (vkd3d-shader:
# "Unsupported assignment type for state VertexShader") and fx_4_0 wrote no shaders: Kodi's
# yuv2rgb effect is written that way, and its video was black. The effect now carries each
# shader, compiled from the effect's own source with the function as entry point
# (test/fxinline-probe.c: D3DCompile fx_4_0 and fx_5_0, reflection of the blobs, D3D10 effect).
#   WINE=/opt/wine-sg/bin/wine test/fxinline-gate.sh
# Mutant: SG_MUTANT_FX_NO_INLINE_SHADERS (libs/vkd3d/libs/vkd3d-shader/fx.c: no shader blobs).
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
T=$(mktemp -d /var/tmp/sg-fxinline.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fxinline-probe.c" -ld3dcompiler -ld3d10 -luuid ||
    { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "(^| )$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
}
check fx5_compiles "fx_5_0: an effect whose pass compiles its shaders inline compiles"
check fx5_vs       "fx_5_0: it carries the vertex shader (vs_4_0, POSITION in)"
check fx5_ps       "fx_5_0: it carries the pixel shader"
check fx4_compiles "fx_4_0: the effect compiles"
check fx4_vs       "fx_4_0: it carries the vertex shader"
check fx4_ps       "fx_4_0: it carries the pixel shader"
if printf '%s\n' "$out" | grep -q "d3d10 device: none"; then echo "SKIP  no D3D10 device: the effect is not created"
else
    check d3d10_effect "D3D10 creates the fx_4_0 effect"
    check pass_vs      "the pass's vertex shader has bytecode"
    check pass_ps      "the pass's pixel shader has bytecode"
fi
check done "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
