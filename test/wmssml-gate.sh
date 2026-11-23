#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Media.SpeechSynthesis SSML (patches/sg/9805): <prosody rate/volume/pitch> (named values as
# Microsoft's SSML documents them, percentages, multiples), <say-as interpret-as="characters"> spelled,
# <sub alias> spoken as its alias -- they were read down to their text. And msttsengine takes a fragment's
# Volume 0 as silence (it was taken as full volume). test/wmssml-probe.c compares durations and loudness.
#   WINE=/opt/wine-sg/bin/wine test/wmssml-gate.sh
# Mutants: SG_MUTANT_SSML_NO_PROSODY (windows.media.speech synthesizer.c),
# SG_MUTANT_TTS_ZERO_VOLUME_LOUD (msttsengine tts.c) -> volume_silent.
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
T=$(mktemp -d /var/tmp/sg-wmssml.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmssml-probe.c" -lole32 -luuid -lruntimeobject || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; timeout -s KILL 120 "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 300 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check plain             "SSML speaks its words"
check rate_x_slow       "prosody rate x-slow speaks at half speed or so"
check rate_x_fast       "prosody rate x-fast speaks faster"
check rate_percent      "prosody rate +100% speaks faster"
check volume_x_soft     "prosody volume x-soft is quieter"
check volume_silent     "prosody volume silent is silence of the words' length"
check pitch_x_high      "prosody pitch x-high speaks"
check nested            "nested prosody"
check say_as_characters "say-as characters spells the word"
check sub_alias         "sub speaks its alias"
check done              "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
