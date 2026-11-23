#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The MP3 encoder MFT (patches/sg/9404; test/mp3enc-probe.c). Wine had none (CLSID_MP3ACMCodecWrapper,
# Windows 8 and later): NAudio's MediaFoundationEncoder.EncodeToMp3 and sink writers to .mp3 found no
# encoder. The probe checks the documented contract (output type first, layer 3 bit rates, the
# MPEGLAYER3WAVEFORMAT tail in MF_MT_USER_DATA, one frame per output sample) and writes and reads back an
# .mp3 through a sink writer: the 1 kHz tone must survive.
#   WINE=/opt/wine-sg/bin/wine test/mp3enc-gate.sh
# Mutants: SG_MUTANT_MP3ENC_BLOCK, SG_MUTANT_MP3ENC_TIME (dlls/winegstreamer/mp3_encoder.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-mp3enc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all,err+winediag} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
LIBS="-lmf -lmfplat -lmfreadwrite -lmfuuid -lmsdmo -lole32 -luuid"
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/mp3enc-probe.c" $LIBS 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
command -v i686-w64-mingw32-gcc >/dev/null &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/probe32.exe" "$HERE/mp3enc-probe.c" $LIBS 2>>"$T/cc.log"
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
    if grep -q "GStreamer doesn't support MP3 encoding" "$T/$p.err"; then
        echo "SKIP: GStreamer has no MP3 encoder here (gstreamer1.0-plugins-good lame)"; exit 77
    fi
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
