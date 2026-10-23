#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The SAPI voice speaks (patches/sg/6508): msttsengine's Speak and GetOutputFormat were stubs and
# the "Wine Default Voice" token named no engine, so SpVoice said nothing. Now eSpeak NG (the
# system's libespeak-ng) synthesizes, in the output's format, with word, sentence and bookmark
# events and the voice's rate. test/ttsvoice-probe.c speaks into WAV files.
#   WINE=/opt/wine-sg/bin/wine test/ttsvoice-gate.sh
# Mutant: SG_MUTANT_TTS_SILENT (msttsengine tts.c).
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
T=$(mktemp -d /var/tmp/sg-ttsvoice.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/ttsvoice-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
command -v "$MINGW32" >/dev/null && TMPDIR=/var/tmp "$MINGW32" -O1 -o "$T/probe32.exe" "$HERE/ttsvoice-probe.c" -lole32 -luuid
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T"/probe*.exe "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
[ -f "$T/probe32.exe" ] && out="$out
$(timeout -s KILL 120 "$WINE" 'C:\probe32.exe' 2>/dev/null | tr -d '\r' | sed 's/^/32:/')"
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check speak          "ISpVoice::Speak through the default voice succeeds"
check audio          "speech is written, at the file's own format (16 kHz)"
check words          "a word boundary event per word"
check word_positions "at each word's place in the text, with its length"
check sentences      "a sentence boundary event per sentence"
check bookmark       "a markup bookmark is an event, after the audio before it"
check rate_fast      "the fastest rate takes less time"
check done           "the probe ran to the end"
if [ -f "$T/probe32.exe" ]; then
    printf '%s\n' "$out" | grep -q '^32:audio=1' && printf '%s\n' "$out" | grep -q '^32:words=1' &&
        pass "a 32-bit program's voice speaks too" || fail "32-bit: $(printf '%s\n' "$out" | grep '^32:' | tr '\n' ' ')"
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
