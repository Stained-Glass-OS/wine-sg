#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct3D 11's multisample format flags on a driver that does 4 samples and not 8 (6097). Wine reported
# D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET/_LOAD/_RESOLVE only when 4 and 8 samples both worked; on
# llvmpipe (virtual machines, machines without a GPU driver: wined3d's OpenGL path) no format had them,
# ANGLE then gave no sample counts for any format, Skia found no renderable format, and every
# Chromium/Electron program's GPU process died three times at start before falling back to software
# drawing (Obsidian's window black until the mouse moved). The flags now follow
# CheckMultisampleQualityLevels: a format some count above 1 works for is multisample-renderable.
#   WINE=/opt/wine-sg/bin/wine test/d3d11msaaflags-gate.sh   (SG_XDISPLAY=N to use Xvfb :N)
# Mutant: SG_MUTANT_MSAA_NEEDS_8 (d3d11). It is caught only where 8 samples do not work (llvmpipe).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in Xvfb $MINGW; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
T=$(mktemp -d /var/tmp/sg-d3d11msaaflags.XXXXXX)
# wined3d, not an installed DXVK
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=;d3d11,dxgi=b" WINESERVER
unset DISPLAY XAUTHORITY
XP=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/d3d11msaaflags-probe.c" -ld3d11 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
if [ -n "${SG_XDISPLAY:-}" ]; then
    Xvfb ":$SG_XDISPLAY" -screen 0 1024x700x24 -nolisten tcp +extension GLX >/dev/null 2>&1 & XP=$!
    echo "$SG_XDISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp +extension GLX 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
[ "$(v done)" = 1 ] || { echo "SKIP: no Direct3D 11 device here"; exit 77; }
[ "$(v rgba8_any)" = 1 ] || { echo "SKIP: no multisampling at all on this driver"; exit 77; }
for f in rgba8 bgra8 rgba16f d24s8; do
    any=$(v ${f}_any)
    [ "$(v ${f}_msrt)" = "$any" ] && pass "$f: multisample render target flag $any, as the sample counts say ($(v ${f}_counts))" \
        || fail "$f: MULTISAMPLE_RENDERTARGET $(v ${f}_msrt) but sample counts '$(v ${f}_counts)'"
    [ "$(v ${f}_msload)" = "$any" ] || fail "$f: MULTISAMPLE_LOAD $(v ${f}_msload), sample counts '$(v ${f}_counts)'"
    [ "$(v ${f}_feature_same)" = 1 ] || fail "$f: CheckFeatureSupport(FORMAT_SUPPORT) differs from CheckFormatSupport"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
