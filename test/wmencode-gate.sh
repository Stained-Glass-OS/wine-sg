#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Windows Media writer encodes (patches/sg/6518): WriteSample's PCM and I420 inputs are encoded
# to the profile's WMA 2 and WMV 2 streams through GStreamer's libav encoders, the file's stream
# properties carry the encoders' formats, and the SDK's own reader plays the file back
# (test/wmencode-probe.c). Needs gstreamer1.0-libav (avenc_wmav2, avenc_wmv2) and
# gstreamer1.0-plugins-ugly (asfdemux, for the reader).
#   WINE=/opt/wine-sg/bin/wine test/wmencode-gate.sh
# Mutants: SG_MUTANT_WMVW_NO_ENCODER (wmvcore writer.c), SG_MUTANT_WG_ENCODER_CAPS_KEPT
# (winegstreamer wg_transform.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for so in libgstlibav.so libgstasf.so; do
    ls /usr/lib/*/gstreamer-1.0/$so >/dev/null 2>&1 || { echo "SKIP: no $so (gstreamer1.0-libav, -plugins-ugly)"; exit 77; }
done
T=$(mktemp -d /var/tmp/sg-wmencode.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmencode-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 180 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check profile        "a profile of a WMA 2 and a WMV 2 stream"
check inputs         "PCM and I420 inputs"
check begin          "BeginWriting starts the encoders"
check write          "WriteSample encodes the sound and the frames"
check end            "EndWriting drains the encoders and finishes the file"
check wma_stream     "the audio stream is WMA 2 with the encoder's codec data"
check wmv_stream     "the video stream is WMV 2 at its size"
check compressed     "the file is compressed"
check reader         "the SDK's reader opens it, two outputs"
check decoded_audio  "and decodes the sound"
check decoded_video  "and the frames"
check done             "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
