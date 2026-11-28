#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Vulkan and Direct3D with no display driver (patches/sg/9559): a service in
# session 0, a program started without a desktop. VK_KHR_win32_surface is the
# host's VK_EXT_headless_surface there, so DXVK creates its instance and a
# Direct3D 11 device works -- as on Windows, where Direct3D works in services.
# Before, vkCreateInstance with VK_KHR_win32_surface failed (-7) and D3D11
# with it: Epic's updater service (EpicGamesLauncher.exe -Commandlet=
# selfupdateinstall -RanAsService) crashed at start and the launcher looped on
# its self-update for good.
#
# Runs test/headlessvk-probe.c with no display at all and DXVK's d3d11/dxgi
# (SG_DXVK_DIR, else /opt/sg-d3d's) on lavapipe when it is there. SKIP without
# DXVK, or when the host's Vulkan has no VK_EXT_headless_surface.
#
#   WINE=/opt/wine-sg/bin/wine test/headlessvk-gate.sh
# Mutant (win32u, -DSG_MUTANT_NO_HEADLESS_SURFACE).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
DXVK=""
for d in "${SG_DXVK_DIR:-}" /opt/sg-d3d/dxvk/x64 "$HERE/../../sg-image/build/d3d-payload/dxvk/x64"; do
    [ -n "$d" ] && [ -f "$d/d3d11.dll" ] && [ -f "$d/dxgi.dll" ] && { DXVK=$d; break; }
done
[ -n "$DXVK" ] || { echo "SKIP: no DXVK payload (SG_DXVK_DIR=.../dxvk/x64)"; exit 77; }
if command -v vulkaninfo >/dev/null && ! vulkaninfo 2>/dev/null | grep -q VK_EXT_headless_surface; then
    echo "SKIP: the host's Vulkan has no VK_EXT_headless_surface"; exit 77
fi
T=$(mktemp -d /var/tmp/sg-headlessvk.XXXXXX)
# no display at all: the null display driver, as a service has
unset DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all DXVK_LOG_LEVEL=none
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winealsa.drv,winepulse.drv=d;explorer.exe=d;d3d11,dxgi,d3d10core=n"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/headlessvk-probe.exe" "$HERE/headlessvk-probe.c" -ld3d11 -luuid \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$DXVK/d3d11.dll" "$DXVK/dxgi.dll" "$WINEPREFIX/drive_c/windows/system32/"
[ -f "$DXVK/d3d10core.dll" ] && cp "$DXVK/d3d10core.dll" "$WINEPREFIX/drive_c/windows/system32/"
timeout -s KILL 90 "$WINE" "$T/headlessvk-probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
grep -q '^vkCreateInstance with win32_surface 0$' "$T/o" && pass "a Vulkan instance with VK_KHR_win32_surface, no display" \
    || fail "the Vulkan instance: $(grep '^vkCreateInstance' "$T/o")"
grep -q '^D3D11CreateDevice 0 ' "$T/o" && pass "a Direct3D 11 device (DXVK)" || fail "Direct3D 11: $(grep '^D3D11' "$T/o")"
grep -q '^pixel ff00ff00$' "$T/o" && pass "it renders: a cleared target reads back green" || fail "rendering: $(grep '^pixel' "$T/o")"
grep -q '^DONE$' "$T/o" || fail "the probe did not finish"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
