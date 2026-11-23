#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Media Foundation AAC encoder (patches/sg/9400; test/aacenc-probe.c). Wine had no audio encoder MFT
# at all: NAudio's MediaFoundationEncoder (MFTranscodeGetAudioOutputAvailableTypes + a sink writer to
# .m4a/.mp4), screen recorders writing H.264 + AAC .mp4 files and transcoders found none. The probe checks
# Microsoft's documented contract of CLSID_AACMFTEncoder (types, MF_MT_USER_DATA, sample times, ADTS,
# errors), decodes the encoder's frames again with CLSID_MSAACDecMFT, and writes and reads back an .mp4
# through a sink writer; the audio must still be the 1 kHz tone it started as.
#   WINE=/opt/wine-sg/bin/wine test/aacenc-gate.sh
# The .mp4 also covers patches/sg/9406 (the MPEG-4 sink asks for its samples; its AAC caps carry codec_data).
# Mutants: SG_MUTANT_AACENC_ASC, SG_MUTANT_AACENC_TIME (dlls/winegstreamer/aac_encoder.c),
# SG_MUTANT_SINK_NO_REQUEST (media_sink.c), SG_MUTANT_AAC_FORMAT_ASC_ONLY (mfplat.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-aacenc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all,err+winediag} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
LIBS="-lmf -lmfplat -lmfreadwrite -lmfuuid -lole32 -luuid"
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/aacenc-probe.c" $LIBS 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
command -v i686-w64-mingw32-gcc >/dev/null &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/probe32.exe" "$HERE/aacenc-probe.c" $LIBS 2>>"$T/cc.log"
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
    if grep -q "GStreamer doesn't support AAC encoding" "$T/$p.err"; then
        echo "SKIP: GStreamer has no AAC encoder here (gstreamer1.0-libav)"; exit 77
    fi
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
