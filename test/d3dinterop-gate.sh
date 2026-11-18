#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct3D 11 <-> WinRT interop for screen capture (wine-sg 7854: dlls/sgd3dinterop, which Wine's
# d3d11.dll forwards to; sg-image dxvk/patches/0011: DXVK's d3d11.dll forwards to it too).
# CreateDirect3D11DeviceFromDXGIDevice and CreateDirect3D11SurfaceFromDXGISurface were missing from
# both d3d11.dlls: OBS's Windows Graphics Capture plugin (libobs-winrt), PotPlayer's and Zoom's
# capture DLLs ended at "unimplemented function". test/d3dinterop-probe.c (64/32-bit) uses them as a
# capture client does -- device wrapped and back, a rendered texture handed out as a frame's
# IDirect3DSurface, its description and pixels read back, IClosable -- against Wine's d3d11
# (wined3d, under Xvfb) and DXVK's (Vulkan): SG_DXVK_X64 / SG_DXVK_X32 (directories with d3d11.dll
# and dxgi.dll; default /opt/sg-d3d/dxvk/x64, x32). A run with no Direct3D 11 device is skipped.
#   WINE=/opt/wine-sg/bin/wine test/d3dinterop-gate.sh
#   (mutant SG_MUTANT_D3DINTEROP_CLOSE_KEEPS, dlls/sgd3dinterop/main.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc Xvfb; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
DX64=${SG_DXVK_X64:-/opt/sg-d3d/dxvk/x64}; DX32=${SG_DXVK_X32:-/opt/sg-d3d/dxvk/x32}
T=$(mktemp -d /var/tmp/sg-d3dinterop.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/d3dinterop-probe.c" -ld3d11 -ldxgi -lruntimeobject -luuid \
        || { echo "FAIL  the probe ($a) did not build"; exit 1; }
done
unset WAYLAND_DISPLAY; export WAYLAND_DISPLAY=sg-no-wayland
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
RC=0; RAN=0
run() {   # label, dir with the probe (and maybe DXVK's DLLs), exe, overrides
    out=$(cd "$2" && timeout -s KILL 180 env WINEDLLOVERRIDES="$WINEDLLOVERRIDES${4:+;$4}" "$WINE" "$2/$3" 2>/dev/null </dev/null | tr -d '\r')
    echo "== $1"; printf '%s\n' "$out" | sed 's/^/      /'
    if printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then RAN=$((RAN + 1))
    elif printf '%s\n' "$out" | grep -qx 'RESULT: SKIP'; then :
    else RC=1; fi
}
mkdir -p "$T/wine64" "$T/wine32"; cp "$T/probe-x86_64.exe" "$T/wine64/"; cp "$T/probe-i686.exe" "$T/wine32/"
run "Wine's d3d11, 64-bit" "$T/wine64" probe-x86_64.exe ""
run "Wine's d3d11, 32-bit" "$T/wine32" probe-i686.exe ""
for b in 64 32; do
    eval d=\$DX$b; a=x86_64; [ $b = 32 ] && a=i686
    if [ -f "$d/d3d11.dll" ] && [ -f "$d/dxgi.dll" ]; then
        mkdir -p "$T/dxvk$b"; cp "$T/probe-$a.exe" "$d/d3d11.dll" "$d/dxgi.dll" "$T/dxvk$b/"
        run "DXVK's d3d11 ($d), $b-bit" "$T/dxvk$b" "probe-$a.exe" "d3d11,dxgi=n"
    else echo "== DXVK $b-bit: no d3d11.dll/dxgi.dll in $d, skipped"; fi
done
[ $RAN -gt 0 ] || { echo "SKIP: no Direct3D 11 device in any run"; exit 77; }
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
