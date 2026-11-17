#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dxva2.dll's decoders are the Direct3D 9 device's (7849), as on Windows, where the driver has them:
# Wine's dxva2 had none, so DXVA2 programs (MPC-HC, VLC's dxva2 mode, FFmpeg's dxva2 hwaccel) all
# decoded on the CPU. A device that has decoders -- DXVK's, sg-d3d -- answers a private interface
# id with its IDirectXVideoDecoderService, and the decoder service hands its calls there:
#   - with such a device: its decoder GUID, its NV12 render target format, its configuration and its
#     decoder object come through
#   - without one (wined3d): no decoder GUID, as before
# A stand-in device and service (no GPU needed); the real decoding is gated in sg-image's
# videodecode-test (DXVA2 clips).
#   WINE=/opt/wine-sg/bin/wine test/dxva2dec-gate.sh   (mutant SG_MUTANT_DXVA2_NO_BACKEND, dxva2/main.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw missing"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dxva2dec.XXXXXX)
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/dxva2dec-probe.c" -ldxva2 -lole32 -luuid \
    || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx "with: guids=1 (its) formats=1 (NV12) configs=1 raw=2 decoder=its" \
    && pass "a device with decoders: its GUID, NV12, its configuration and its decoder come through dxva2" \
    || fail "with a device that has decoders: $(printf '%s\n' "$out" | grep '^with:' || echo none)"
printf '%s\n' "$out" | grep -qx "without: guids=0" \
    && pass "a device without them: no decoder GUID (programs decode on the CPU)" \
    || fail "without: $(printf '%s\n' "$out" | grep '^without:' || echo none)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
