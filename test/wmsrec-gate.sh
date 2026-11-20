#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Media.SpeechRecognition hears speech (wine-sg 8206, W-SPEECH): RecognizeAsync used to give an empty
# TimeoutExceeded result (no engine). Now the system's own engine (sg-dictate: Parakeet, on the CPU) listens
# until the speaker pauses. The gate gives it a recording of "open the door" (espeak-ng) as its microphone
# (sg-dictate's SG_DICTATE_AUDIO_FILE), on a private runtime directory (never the person's audio). The probe
# (test/wmsrec-probe.c): dictation -> Success, High, the words; a list constraint with the phrase -> that
# phrase as written, its constraint; a list without it -> Rejected, no text.
#   WINE=/opt/wine-sg/bin/wine test/wmsrec-gate.sh
# Needs sg-dictate with its model (sg-speech-model-parakeet) and espeak-ng; SKIP without.
# Mutants (windows.media.speech/recognizer.c): SG_MUTANT_WMS_RECOGNIZE (no engine asked), SG_MUTANT_WMS_NO_MATCH.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0; XP=
for need in "$MINGW" espeak-ng Xvfb; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -x /usr/bin/sg-dictate ] || { echo "SKIP: no sg-dictate"; exit 77; }
sg-dictate --status 2>/dev/null | grep -q 'MODEL installed' || { echo "SKIP: the speech model is not installed"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wmsrec.XXXXXX)
cleanup() { [ -n "$XP" ] && kill "$XP" 2>/dev/null; WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmsrec-probe.c" -lole32 -lruntimeobject -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
espeak-ng -v en-us -s 150 -w "$T/door.wav" "open the door" || { echo "FAIL  espeak-ng made no recording"; exit 1; }
unset WAYLAND_DISPLAY PULSE_SERVER PIPEWIRE_REMOTE
Xvfb -displayfd 3 -screen 0 640x480x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
[ -s "$T/display" ] || { echo "SKIP: Xvfb did not start"; exit 77; }
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(SG_DICTATE_AUDIO_FILE="$T/door.wav" timeout -s KILL 600 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {
    if printf '%s\n' "$out" | grep -qx "$1=$2"; then printf 'PASS  %s\n' "$3"
    else printf 'FAIL  %s: wanted %s=%s, got %s\n' "$3" "$1" "$2" "$(printf '%s\n' "$out" | grep "^$1=" || echo none)"; RC=1; fi
}
case "$(printf '%s\n' "$out" | grep '^recognizer=')" in
    "recognizer=0") ;;
    *) echo "SKIP: no recognizer here (an audio capture device is needed to make one)"; exit 77;;
esac
expect dictation '0 0 \[[Oo]pen the door\.\?\] 0' "dictation: Success, High confidence, the words said"
expect list '0 0 \[open the door\] 1' "a list with the phrase: that phrase as written, its constraint"
expect rejected '0 3 \[\] 0' "a list without it: Rejected, no text"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
