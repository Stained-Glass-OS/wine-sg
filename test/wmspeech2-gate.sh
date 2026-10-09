#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"

# windows.media.speech part 2, patch 2811, on Xvfb: test/wmspeech2-probe.c checks the
# recognizer's CurrentLanguage/SystemSpeechLanguage/supported languages,
# Timeouts and UIOptions, TrySetSystemSpeechLanguageAsync, RecognizeAsync and
# RecognizeWithUIAsync (no engine: a TimeoutExceeded result), the StateChanged
# event, StopRecognitionAsync and Close, then the synthesizer's Options, Voice,
# DefaultVoice and Close. The recognizer part SKIPs inside the probe when no
# audio capture device lets a recognizer be created.
#
#   WINE=/opt/wine-sg/bin/wine test/wmspeech2-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (windows.media.speech, -DSG_MUTANT_x): WMS_TIMEOUTS (initial silence
# timeout starts at 0), WMS_STATE_EVENT (StateChanged never fires), WMS_RECOGNIZE
# (the result says Success), WMS_TRYSET (the language change reports TRUE),
# WMS_OPTIONS_RANGE (out-of-range synthesizer options are accepted), WMS_CLOSE
# (Close does not close the synthesizer).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wmspeech2.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/wmspeech2-probe.exe" "$HERE/wmspeech2-probe.c" -lole32 -lruntimeobject -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wmspeech2-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" wmspeech2-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
