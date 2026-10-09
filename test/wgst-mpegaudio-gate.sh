#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winegstreamer's MPEG audio decoder IMpegAudioDecoder settings (patches/sg/2861),
# on Xvfb: test/wgst-mpegaudio-probe.c checks the defaults, the valid and the
# rejected values of FrequencyDivider, DecoderAccuracy, Stereo, DecoderWordSize,
# IntegerDecode and DualMode, NULL pointers, and get_AudioFormat unconnected and
# connected to a fake source pin. No FIXME may be logged by those functions.
# SKIPs when GStreamer cannot decode MPEG audio (the filter does not exist).
#
#   WINE=/opt/wine-sg/bin/wine test/wgst-mpegaudio-gate.sh
# Mutants (winegstreamer, -DSG_MUTANT_x): MPEGDIV (any frequency divider accepted),
# MPEGAUDIOFMT (get_AudioFormat succeeds while unconnected), MPEGDEFAULT (word size
# defaults to 8).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wgstmpa.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+quartz WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -Wno-format -o "$T/wgst-mpegaudio-probe.exe" "$HERE/wgst-mpegaudio-probe.c" -lole32 -luuid -lstrmiids -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wgst-mpegaudio-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" wgst-mpegaudio-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:quartz:mpeg_audio_decoder_' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -q "^SKIP" "$T/probe.out" && exit 77
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
