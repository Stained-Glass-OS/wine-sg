#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# VK_KHR_external_memory_win32 (6261): Vulkan memory exported as Win32 handles over the host's
# opaque fds (win32u keeps the fd behind a D3DKMT resource). An image cleared in one VkDevice is
# imported by a second device through a KMT handle, and by another process through an NT handle
# (DuplicateHandle); both read its pixels back. DXVK shares Direct3D 11 textures so: Qt WebEngine
# (GOG GALAXY, calibre) and Chromium hand their frames over as shared textures, which Wine 10 lost.
# Timeline semaphores are shared the same way (VK_KHR_external_semaphore_win32, D3DKMT sync objects):
# DXVK's keyed mutex needs them. Lavapipe has no opaque fd semaphores, so a second run uses the
# host's own drivers (a GPU) and does the semaphore checks there; VKSHARE_SEMAPHORES=1 requires them.
# Runs on lavapipe (software Vulkan) under Xvfb. test/vkshare-probe.c.
# Mutants: SG_MUTANT_NO_WIN32_MEMORY, SG_MUTANT_NO_WIN32_SEMAPHORE, SG_MUTANT_SEM_IMPORT_FORGETS (dlls/winevulkan/vulkan.c),
# SG_MUTANT_KMT_VK_NOFD, SG_MUTANT_KMT_SYNC_NOFD (dlls/win32u/d3dkmt.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for t in xvfb-run x86_64-w64-mingw32-gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -f /usr/include/vulkan/vulkan.h ] || { echo "SKIP: no Vulkan headers (libvulkan-dev)"; exit 77; }
LVP=$(ls /usr/share/vulkan/icd.d/lvp_icd*.json 2>/dev/null | head -1)
[ -n "$LVP" ] || { echo "SKIP: no lavapipe (mesa-vulkan-drivers)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-vkshare.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
export VK_ICD_FILENAMES="$LVP" VK_DRIVER_FILES="$LVP"
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/inc"; cp -r /usr/include/vulkan "$T/inc/"; [ -d /usr/include/vk_video ] && cp -r /usr/include/vk_video "$T/inc/"
x86_64-w64-mingw32-gcc -O1 -I"$T/inc" -o "$T/vkshare.exe" "$HERE/vkshare-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
B32=""
if command -v i686-w64-mingw32-gcc >/dev/null; then
    i686-w64-mingw32-gcc -O1 -I"$T/inc" -o "$T/vkshare32.exe" "$HERE/vkshare-probe.c" || { echo "FAIL  32-bit probe did not build"; exit 1; }
    B32=1
fi
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/vkshare.exe" "$WINEPREFIX/drive_c/"
[ -n "$B32" ] && cp "$T/vkshare32.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" vkshare.exe 2>"$T/err.txt" </dev/null | tr -d '\r' > "$T/probe.out"
if [ -n "$B32" ]; then timeout -s KILL 240 "$WINE" vkshare32.exe 2>>"$T/err.txt" </dev/null | tr -d '\r' > "$T/probe32.out"; fi
# the host's own drivers (a GPU), for the semaphores lavapipe cannot share
unset VK_ICD_FILENAMES VK_DRIVER_FILES
timeout -s KILL 240 "$WINE" vkshare.exe 2>>"$T/err.txt" </dev/null | tr -d '\r' > "$T/probegpu.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
sed 's/^/  [64] /' "$T/probe.out"
RC=0
grep -qx 'RESULT: PASS' "$T/probe.out" || RC=1
if [ -n "$B32" ]; then
    sed 's/^/  [32] /' "$T/probe32.out"
    grep -qx 'RESULT: PASS' "$T/probe32.out" || RC=1
fi
# the semaphore checks are required when the host's own first device can do them
NEED_SEM="${VKSHARE_SEMAPHORES:-}"
if [ -z "$NEED_SEM" ] && command -v gcc >/dev/null && gcc -o "$T/native" "$HERE/vkshare-native.c" -lvulkan 2>/dev/null &&
   (unset VK_ICD_FILENAMES VK_DRIVER_FILES; timeout 30 "$T/native" >/dev/null 2>&1); then
    NEED_SEM=1
    echo "NOTE  the host's driver shares timeline semaphores as fds: the semaphore checks are required"
fi
if grep -q 'RESULT:' "$T/probegpu.out"; then
    sed 's/^/  [gpu] /' "$T/probegpu.out"
    grep -qx 'RESULT: PASS' "$T/probegpu.out" || RC=1
    if grep -q '^SKIP  VK_KHR_external_semaphore_win32' "$T/probegpu.out"; then
        echo "NOTE  no driver here shares semaphores: the semaphore checks were skipped"
        [ -n "$NEED_SEM" ] && RC=1
    fi
else
    echo "NOTE  the host's drivers gave no Vulkan device"
    [ -n "$NEED_SEM" ] && RC=1
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
