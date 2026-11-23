#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# win32u: D3DKMTQueryAdapterInfo(KMTQAITYPE_QUERYREGISTRY) (patches/sg/9802) -- a value of the
# adapter's driver key (ADAPTERKEY) or of its service's key (SERVICEKEY), with the BUFFER_OVERFLOW size
# protocol, the type check, TranslatePath and STATUS_FAIL for a missing value. It was not supported
# (FIXME "type 48 not supported" ~60 times in each of 7 Electron programs: the Vulkan loader Chromium
# bundles asks every adapter for VulkanDriverName). 64- and 32-bit, under Xvfb.
# test/kmtqueryreg-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/kmtqueryreg-gate.sh
# Mutants (dlls/win32u/d3dkmt.c): SG_MUTANT_QUERYREGISTRY_ANY_TYPE (a mistyped value answered);
# without the patch the query is STATUS_NOT_IMPLEMENTED.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
DPY=${KMTQUERYREG_DPY:-351}
for need in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc Xvfb xdpyinfo; do
    command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
case "$DPY" in 0|0.*) echo "FAIL  refusing display :0"; exit 1 ;; esac
T=$(mktemp -d /var/tmp/sg-kmtqueryreg.XXXXXX)
XP=
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/kmtqueryreg-probe.c" -lgdi32 -luser32 -ladvapi32 &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/kmtqueryreg-probe.c" -lgdi32 -luser32 -ladvapi32 ||
    { echo "FAIL  the probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
unset WAYLAND_DISPLAY XAUTHORITY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 40 ]; do python3 -c "import time; time.sleep(0.25)"; i=$((i + 1)); done
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 120 "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
RC=0
for p in p64 p32; do
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/  $p /"
    printf '%s\n' "$out" | tail -n1 | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
