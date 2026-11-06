#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct3D 11 textures shared between processes under DXVK (6261), with keyed mutexes: the G-SHARED
# probe (test/g-shared-d3d11-probe.c) with DXVK's d3d11/dxgi as native DLLs. DXVK shares the Vulkan
# memory of a texture as a Win32 handle (VK_KHR_external_memory_win32) and the fence of its keyed
# mutex as a Win32 semaphore handle (VK_KHR_external_semaphore_win32); winevulkan backs both with the
# host's opaque fds, win32u keeps them behind the D3DKMT objects. Qt WebEngine (GOG GALAXY, calibre)
# and Chromium hand their frames over so. A texture is drawn in one process and handed to another with
# key 1 and back with key 2, by a legacy handle and by an inherited NT handle, 64-bit and 32-bit.
# Also covered: the DWORD milliseconds * -10000 timeout of DXVK's AcquireSync, the SECURITY_ATTRIBUTES
# it passes as a security descriptor (inheritance), and DXVK opening its own memory with no buffers.
# Needs a GPU (lavapipe has no opaque fd semaphores): SKIP without /dev/dri render nodes or DXVK.
# What DXVK itself does not implement (GetSharedResourceAdapterLuid, OpenSharedResourceByName, abandoned
# mutexes) is left out.
#   WINE=/opt/wine-sg/bin/wine test/g-shared-dxvk-gate.sh
# Mutants: SG_MUTANT_NO_WIN32_SEMAPHORE (dlls/winevulkan/vulkan.c),
# SG_MUTANT_KMT_DXVK_TIMEOUT, SG_MUTANT_KMT_SA_AS_SD, SG_MUTANT_KMT_OPEN_EMPTY_BUFFERS, SG_MUTANT_KMT_SYNC_NOFD
# (dlls/win32u/d3dkmt.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for need in Xvfb xdpyinfo x86_64-w64-mingw32-gcc; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
ls /dev/dri/renderD* >/dev/null 2>&1 || { echo "SKIP: no GPU render node"; exit 77; }
DXVK=""
for d in "${SG_DXVK_DIR:-}" /opt/sg-d3d/dxvk "$HERE/../../sg-image/build/d3d-payload/dxvk" \
         "${SG_REAL_HOME:-/nonexistent}/Stained-Glass-OS/sg-image/build/d3d-payload/dxvk"; do
    [ -n "$d" ] && [ -f "$d/x64/d3d11.dll" ] && [ -f "$d/x64/dxgi.dll" ] && { DXVK=$d; break; }
done
[ -n "$DXVK" ] || { echo "SKIP: no DXVK payload (SG_DXVK_DIR=.../dxvk with x64/ x32/)"; exit 77; }
unset VK_ICD_FILENAMES VK_DRIVER_FILES WAYLAND_DISPLAY

DPY=255
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
T=$(mktemp -d /var/tmp/sg-gshareddxvk.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM

inc_list="-idirafter ${WINE_INCLUDE:-/nonexistent} -idirafter $(dirname "$WINE")/include/wine/windows -idirafter /opt/wine-sg/include/wine/windows"
archs="x86_64-w64-mingw32-gcc"
[ -f "$DXVK/x32/d3d11.dll" ] && command -v i686-w64-mingw32-gcc >/dev/null && archs="$archs i686-w64-mingw32-gcc"
n=0
for cc in $archs; do
    n=$((n + 1))
    TMPDIR=/var/tmp "$cc" -O2 -mwindows $inc_list -o "$T/probe$n.exe" "$HERE/g-shared-d3d11-probe.c" \
        -ld3d11 -ldxgi -ld3d10 -luuid -ldxguid 2>"$T/cc.err" \
        || { cat "$T/cc.err"; echo "FAIL  the probe did not build ($cc)"; exit 1; }
done

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$DXVK"/x64/d3d11.dll "$DXVK"/x64/dxgi.dll "$WINEPREFIX/drive_c/windows/system32/"
[ -f "$DXVK/x64/d3d10core.dll" ] && cp "$DXVK/x64/d3d10core.dll" "$WINEPREFIX/drive_c/windows/system32/"
if [ -f "$DXVK/x32/d3d11.dll" ]; then
    cp "$DXVK"/x32/d3d11.dll "$DXVK"/x32/dxgi.dll "$WINEPREFIX/drive_c/windows/syswow64/"
    [ -f "$DXVK/x32/d3d10core.dll" ] && cp "$DXVK/x32/d3d10core.dll" "$WINEPREFIX/drive_c/windows/syswow64/"
fi
export WINEDLLOVERRIDES="$WINEDLLOVERRIDES;d3d11,dxgi,d3d10core=n" DXVK_LOG_LEVEL=none

# what DXVK does not do itself
ALLOWED='GetSharedResourceAdapterLuid|NT: a handle of nothing|legacy: a handle of nothing|a mutex whose holder exited is abandoned|^FAIL  named: |OpenSharedResourceByName'
rc=0
i=1
while [ $i -le $n ]; do
    timeout -s KILL 150 "$WINE" "$T/probe$i.exe" >"$T/out.raw" 2>/dev/null </dev/null
    tr -d '\r' <"$T/out.raw" >"$T/out"
    w=$([ $i = 1 ] && echo 64 || echo 32)
    for want in "legacy: key 2 comes back" "legacy: what the other process drew" \
                "NT handle: key 2 comes back" "NT handle: what the other process drew" \
                "OpenSharedResource1 in the creating process" "second device opens it" "and sees what the first drew"; do
        if grep -q "^PASS  $want" "$T/out"; then echo "PASS  [$w] $want"; else echo "FAIL  [$w] $want"; rc=1; fi
    done
    c=$(grep -c '^PASS  child: the pixels that came with key 1' "$T/out")
    if [ "$c" -ge 2 ]; then echo "PASS  [$w] the other process saw the pixels with key 1, both handle kinds"
    else echo "FAIL  [$w] the other process saw the pixels with key 1 $c times, want 2"; rc=1; fi
    other=$(grep '^FAIL' "$T/out" | grep -vE "$ALLOWED")
    if [ -n "$other" ]; then echo "$other" | sed "s/^/  [$w] /"; echo "FAIL  [$w] other checks failed"; rc=1; fi
    i=$((i + 1))
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
