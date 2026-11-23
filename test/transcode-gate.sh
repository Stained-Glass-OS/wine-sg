#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The transcode API (patches/sg/9405; test/transcode-probe.c): MFCreateTranscodeTopology was E_NOTIMPL
# (Zoom's media adapter imports it; 7853) and MFCreateTranscodeSinkActivate a stub. The probe follows
# Microsoft's tutorials: a WAV source from the source resolver, a profile with an audio type from
# MFTranscodeGetAudioOutputAvailableTypes and a container, the partial topology run by a Media Session to
# its end -- AAC in MPEG-4, WMA in ASF and MP3, at the source's rate and resampled -- and the files are read
# back: they must hold the source's 1 kHz tone. Also the documented errors and the sink activation
# object's IMFTranscodeSinkInfoProvider. The encoders keeping the types a topology loader sets again are
# patches/sg/9407.
#   WINE=/opt/wine-sg/bin/wine test/transcode-gate.sh
# Mutants: SG_MUTANT_TRANSCODE_NO_ENCODER (dlls/mf/sg_transcode.c), SG_MUTANT_MP3ENC_DROP_INPUT
# (dlls/winegstreamer/mp3_encoder.c, 9407).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-transcode.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all,err+winediag} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
LIBS="-lmf -lmfplat -lmfreadwrite -lmfuuid -lole32 -luuid"
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/transcode-probe.c" $LIBS 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
command -v i686-w64-mingw32-gcc >/dev/null &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/probe32.exe" "$HERE/transcode-probe.c" $LIBS 2>>"$T/cc.log"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
for p in probe probe32; do
    [ -f "$T/$p.exe" ] || continue
    echo "== $p"
    timeout -s KILL 300 "$WINE" "$T/$p.exe" 2>"$T/$p.err" </dev/null > "$T/$p.raw"
    tr -d '\r' < "$T/$p.raw" | tee "$T/$p.out"
    if grep -q "GStreamer doesn't support AAC encoding" "$T/$p.err"; then
        echo "SKIP: GStreamer has no AAC encoder here (gstreamer1.0-libav)"; exit 77
    fi
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
