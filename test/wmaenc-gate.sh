#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows Media Audio encoder (patches/sg/9403; test/wmaenc-probe.c). Wine had no WMA encoder MFT or DMO
# (CLSID_CWMAEncMediaObject): NAudio's MediaFoundationEncoder.EncodeToWma, programs writing .wma through a
# sink writer or the WMContainer objects, and DirectShow graphs with the WMA encoder DMO found none. The
# probe checks the documented contract (input type first, WMA 8 output types for it, one block per output
# sample), and writes and reads back a .wma through a sink writer and mf.dll's ASF sink; the audio must
# still be the 1 kHz tone it started as.
#   WINE=/opt/wine-sg/bin/wine test/wmaenc-gate.sh
# Mutants: SG_MUTANT_WMAENC_CODEC_DATA, SG_MUTANT_WMAENC_TIME, SG_MUTANT_WMAENC_ZERO_CODEC_DATA (9407)
# (dlls/winegstreamer/wma_encoder.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wmaenc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all,err+winediag} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
LIBS="-lmf -lmfplat -lmfreadwrite -lmfuuid -lmsdmo -lole32 -luuid"
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/wmaenc-probe.c" $LIBS 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
command -v i686-w64-mingw32-gcc >/dev/null &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/probe32.exe" "$HERE/wmaenc-probe.c" $LIBS 2>>"$T/cc.log"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
for p in probe probe32; do
    [ -f "$T/$p.exe" ] || continue
    echo "== $p"
    timeout -s KILL 180 "$WINE" "$T/$p.exe" 2>"$T/$p.err" </dev/null > "$T/$p.raw"
    tr -d '\r' < "$T/$p.raw" | tee "$T/$p.out"
    if grep -q "GStreamer doesn't support WMA encoding" "$T/$p.err"; then
        echo "SKIP: GStreamer has no WMA encoder here (gstreamer1.0-libav)"; exit 77
    fi
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
