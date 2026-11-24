#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# vulkan-1's VK_KHR_display entry points (patches/sg/9554), 64- and 32-bit:
# test/vkdisplay-probe.c. They were spec stubs (a call crashed the program);
# now they answer as the Khronos loader does when no driver offers the
# extension. Uses the host's Vulkan device when there is one.
#
#   WINE=/opt/wine-sg/bin/wine test/vkdisplay-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (vulkan-1/vulkan.c): SG_MUTANT_DISPLAY_NO_COUNT.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-vkdisplay.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -Wl,--build-id -o "$T/p64.exe" "$HERE/vkdisplay-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -Wl,--build-id -o "$T/p32.exe" "$HERE/vkdisplay-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
