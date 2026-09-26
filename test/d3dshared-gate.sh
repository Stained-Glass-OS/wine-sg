#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Shared D3D11 textures under DXVK do not crash (patches/sg/0361).
#
# DXVK 3.x calls vkGetMemoryWin32HandleKHR for a texture created with
# MISC_SHARED / SHARED_NTHANDLE -- as Chromium, Qt WebEngine and WebView2 do
# for GPU compositing -- and our winevulkan did not provide
# VK_KHR_external_memory_win32, so the process died (GOG Galaxy at start).
# The probe creates both kinds of shared texture through DXVK and must
# survive.
#
#   DXVK_DIR=/path/to/dxvk (with x64/, x32/) WINE=... test/d3dshared-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DXVK_DIR="${DXVK_DIR:-}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: no Xvfb"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -n "$DXVK_DIR" ] && [ -f "$DXVK_DIR/x64/d3d11.dll" ] || { echo "SKIP: set DXVK_DIR to a DXVK build (x64/d3d11.dll)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-d3dshared.XXXXXX)
# A display of our own -- never the desktop's.
DN=$(( 400 + $$ % 200 ))
Xvfb ":$DN" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 &
XPID=$!
export DISPLAY=":$DN" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;d3d11,dxgi=n"
cleanup() { "$WINESERVER" -k 2>/dev/null; kill "$XPID" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
sleep 2
"$MINGW" -O2 -o "$T/d3dshared-probe.exe" "$HERE/d3dshared-probe.c" -ld3d11 -ldxgi -ldxguid -luuid || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$DXVK_DIR/x64/d3d11.dll" "$DXVK_DIR/x64/dxgi.dll" "$WINEPREFIX/drive_c/windows/system32/"
out=$(timeout -s KILL 120 "$WINE" "$T/d3dshared-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
case "$out" in *"d3d11=native"*) pass "DXVK's d3d11 answered" ;; *) fail "not DXVK's d3d11: $(printf '%s\n' "$out" | head -1)" ;; esac
case "$out" in *"device=00000000"*) pass "a device was created" ;; *) echo "SKIP: no Vulkan device for DXVK here"; exit 77 ;; esac
printf '%s\n' "$out" | grep -q '^shared0 create=00000000' && pass "a MISC_SHARED texture is created" || fail "MISC_SHARED texture"
printf '%s\n' "$out" | grep -qx 'survived=1' && pass "the process survived creating and sharing both kinds" || fail "the process died creating shared textures"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
