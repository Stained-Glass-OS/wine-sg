#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A voice per language, each speaking its own (patches/sg/9418).
#
# There was one voice, English: a program choosing its voice by language (SAPI's
# "Language=407", Windows.Media.SpeechSynthesis' AllVoices) found none for German,
# and German text through it came out in English sounds. Each language eSpeak NG
# speaks now has a voice token (Language, Name, Gender attributes), the engine
# speaks the token's language for text that names none, and with no choice of
# the user's the default voice is the display language's, as on Windows.
#
# The probe (test/sapivoices-probe.c) finds the voices by language, speaks German
# through the German voice (~ the same as <lang langid=407> through the English
# one, unlike the English voice's reading), and reads the default voice in
# prefixes made under de_DE and en_US.
#   WINE=/opt/wine-sg/bin/wine test/sapivoices-gate.sh
# Mutants: SG_MUTANT_TTS_VOICE_LANG_IGNORED (msttsengine tts.c),
#          SG_MUTANT_SAPITOK_NO_UI_VOICE (sapi token.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-sapivoices.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;${WINEDLLOVERRIDES:-}"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/sapivoices-probe.c" -lole32 -loleaut32 -luuid -lshlwapi \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
# to a file: services started with the prefix keep a pipe open
timeout -s KILL 120 "$WINE" 'C:\probe.exe' >"$T/out" 2>/dev/null
out=$(tr -d '\r' < "$T/out")
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check german_voice  "a German voice (Language=407, its Name)"
check french_voice  "a French voice (Language=40C)"
check english_voice "the English voice is still there (Language=409)"
check voice_count   "a voice for each language the synthesizer speaks"
check speaks        "the German voice speaks"
check own_language  "plain text through the German voice is spoken in German"
check not_english   "not in the English voice's sounds"
# the default voice is the display language's (a prefix made under each)
default_voice() {
    loc=$1; want=$2; P="$T/prefix-$loc"
    WINEPREFIX="$P" LANG=$loc LC_ALL=$loc timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; WINEPREFIX="$P" "$WINESERVER" -w
    cp "$T/probe.exe" "$P/drive_c/"
    WINEPREFIX="$P" LANG=$loc LC_ALL=$loc timeout -s KILL 60 "$WINE" 'C:\probe.exe' default >"$T/def.out" 2>/dev/null
    d=$(tr -d '\r' < "$T/def.out"); echo "      $loc: $d"
    case "$d" in *"Tokens\\$want ("*) pass "the default voice under $loc is the display language's ($want)" ;;
        *) fail "the default voice under $loc ($d, want $want)" ;; esac
    WINEPREFIX="$P" "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$P"
}
default_voice de_DE.UTF-8 "Stained Glass Voice de-DE"
default_voice en_US.UTF-8 "Wine Default Voice"
exit $RC
