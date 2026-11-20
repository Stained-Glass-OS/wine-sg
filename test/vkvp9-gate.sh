#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# VK_KHR_video_decode_vp9 for Windows programs (7848): DXVK's D3D11 VP9 video decoders (sg-d3d,
# dxvk/patches/0009) need it, and winevulkan's thunks, made from an older Vulkan registry, did not
# know it -- it was never offered. Now, on a host whose driver decodes VP9 with Vulkan:
#   - a 64-bit program is offered it and gets a device with it and its videoDecodeVP9 feature
#   - (8203) a 32-bit program too: its structures are converted (feature, VP9 profile, decode capabilities,
#     picture info): it gets the device, the feature reported, the VP9 decode capabilities (a level) and a
#     video session for VP9 profile 0; and so does a 64-bit one
# Headless: its own Xvfb, the host's Vulkan driver (not lavapipe), a private runtime directory.
#   WINE=/opt/wine-sg/bin/wine test/vkvp9-gate.sh   (mutants SG_MUTANT_VP9_NOT_OFFERED, SG_MUTANT_VP9_WOW64_HIDDEN
#   in winevulkan/vulkan.c; SG_MUTANT_VP9_NO_WOW64_CONVERSION in vulkan_thunks.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in Xvfb vulkaninfo x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
# the host's own driver: scratch-home points Vulkan at lavapipe, which decodes no video
unset VK_ICD_FILENAMES VK_DRIVER_FILES
export XDG_DATA_DIRS=/usr/local/share:/usr/share   # where the loader finds the drivers
vulkaninfo 2>/dev/null | grep -q VK_KHR_video_decode_vp9 || { echo "SKIP: the host's Vulkan driver does not decode VP9"; exit 77; }
T=$(mktemp -d /var/tmp/sg-vkvp9.XXXXXX)
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/vkvp9-probe.c" || { fail "probe did not build"; exit 1; }
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/vkvp9-probe.c" || { fail "32-bit probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 640x480x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
[ -s "$T/display" ] || { echo "SKIP: Xvfb did not start"; exit 77; }
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
o64=$(timeout 120 "$WINE" "$T/p64.exe" 2>/dev/null | tr -d '\r'); o32=$(timeout 120 "$WINE" "$T/p32.exe" 2>/dev/null | tr -d '\r')
echo "      64-bit: $o64"; echo "      32-bit: $o32"
for b in 64 32; do
    if [ "$b" = 64 ]; then o=$o64; else o=$o32; fi
    printf '%s\n' "$o" | grep -qx "vp9 offered=1 device=0" && pass "a $b-bit program is offered VK_KHR_video_decode_vp9 and gets a device with its feature" \
        || fail "$b-bit: '$(printf '%s' "$o" | head -1)' (wanted offered, device 0)"
    printf '%s\n' "$o" | grep -Eqx "vp9 feature=1 caps=0 level=[0-9]+ session=0" \
        && pass "$b-bit: the feature reported, VP9 decode capabilities with a level, a VP9 video session" \
        || fail "$b-bit: '$(printf '%s\n' "$o" | sed -n 2p)' (wanted feature=1 caps=0 level=N session=0)"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
