#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Media.SpeechSynthesis speaks (patches/sg/6510): the WinRT synthesizer had no voices
# and handed out empty stream objects. Now SAPI's voices are its voices and the voice's engine
# (msttsengine, eSpeak NG, 6508) speaks into a WAV stream with word and sentence markers, from
# text or SSML. test/wmsynth-probe.c reads the stream back as a program does.
#   WINE=/opt/wine-sg/bin/wine test/wmsynth-gate.sh
# Mutants: SG_MUTANT_WMS_NO_VOICES, SG_MUTANT_WMS_SILENT, SG_MUTANT_WMS_NO_AUDIO_STREAM
# (windows.media.speech synthesizer.c).
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
ls /usr/lib/*/libespeak-ng.so.1 >/dev/null 2>&1 || { echo "SKIP: no libespeak-ng1"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wmsynth.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmsynth-probe.c" -lole32 -luuid -lruntimeobject || { fail "probe did not build"; exit 1; }
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
check voices           "AllVoices lists the installed (SAPI) voices"
check default_voice    "DefaultVoice has a name, a language and an id"
check synth_voice      "a new synthesizer speaks with the default voice"
check speak            "SynthesizeTextToStreamAsync completes"
check wav              "the stream is a PCM WAV file, read through IInputStream into a Buffer"
check audio            "it holds the spoken text (length, loudness)"
check no_markers       "no markers unless the options ask for them"
check content_type     "its content type is audio/wav"
check ssml             "SSML speaks its text as the same text does"
check ssml_break       "an SSML break is a pause of its length"
check ssml_rejected    "text that is not SSML fails the operation (format error)"
check word_markers     "a SpeechWord marker per word, with the word"
check marker_times     "word marker times rise within the audio"
check sentence_markers "a SpeechSentence marker per sentence, with the sentence"
check rate             "SpeakingRate 3 speaks faster"
check done             "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
