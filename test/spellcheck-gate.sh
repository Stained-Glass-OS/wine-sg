#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Spell Checking API (patches/sg/7010): msspellcheckingfacility.dll registers
# CLSID_SpellCheckerFactory over the system's hunspell dictionaries (libhunspell loaded at run
# time, /usr/share/hunspell). HexChat's spell checker (enchant's "win8" provider) failed with
# "init_enchant_provider returned invalid provider". The probe (test/spellcheck-probe.c, 64- and
# 32-bit) checks languages, Check (misspelled words, a repeated word, addresses and numbers let
# be, apostrophes), Suggest, Ignore, Add (kept in %APPDATA%\Microsoft\Spelling\en-US\default.dic
# for the next checker), Remove, AutoCorrect, the change event and a registered exclusion list.
#   WINE=/opt/wine-sg/bin/wine test/spellcheck-gate.sh
# Mutant: SG_MUTANT_SPELL_ALWAYS_RIGHT (dlls/msspellcheckingfacility/unixlib.c).
set -u
unset DISPLAY
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -r /usr/share/hunspell/en_US.dic ] || { echo "SKIP: hunspell-en-us not installed"; exit 77; }
ls /usr/lib/*/libhunspell-1.7.so.0 >/dev/null 2>&1 || { echo "SKIP: libhunspell-1.7-0 not installed"; exit 77; }
T=$(mktemp -d /var/tmp/sg-spellcheck.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for b in 64 32; do
    cc=x86_64-w64-mingw32-gcc; [ $b = 32 ] && cc=i686-w64-mingw32-gcc
    TMPDIR=/var/tmp $cc -O1 -o "$T/probe$b.exe" "$HERE/spellcheck-probe.c" -lole32 -luuid || { fail "probe ($b-bit) did not build"; exit 1; }
done
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
for b in 64 32; do
    rm -rf "$WINEPREFIX"/drive_c/users/*/AppData/Roaming/Microsoft/Spelling
    out=$(timeout -s KILL 180 "$WINE" "C:\\probe$b.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $b-bit: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "(^| )$1=1( |$)"; then pass "$b-bit: $2"
        else fail "$b-bit: $2 ($(printf '%s\n' "$out" | grep -Eo "(^| )$1=[^ ]*" | head -1 || echo none))"; fi
    }
    check created            "CoCreateInstance(CLSID_SpellCheckerFactory) works"
    check supported          "en-US is supported"
    check unsupported        "a language with no dictionary is not"
    check languages          "SupportedLanguages lists en-US"
    check bare_language      "a bare language (\"en\", as HexChat asks) is its installed variant (en-US)"
    check create_unsupported "CreateSpellChecker refuses a language with no dictionary"
    check checker            "CreateSpellChecker makes an en-US checker (tag case does not matter)"
    check tag                "its LanguageTag is en-US"
    check check              "Check finds the misspelled words and the repeated word, with their actions"
    check names_skipped      "addresses, links and words with digits are not checked"
    check apostrophes        "words with straight and typographic apostrophes are right"
    check suggest            "Suggest offers the right spelling"
    check suggest_right      "Suggest of a right word answers S_FALSE with the word"
    check ignored            "an ignored word is not an error"
    check added              "an added word is not an error"
    check events             "the change event fires for Ignore and Add"
    check autocorrect        "an autocorrect pair is a replace error"
    check persisted          "a new checker knows the added word but not the session's ignored one"
    check removed            "a removed word is an error again"
    check registered         "RegisterUserDictionary takes an .exc file"
    check excluded           "a registered excluded word is an error"
    check done               "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
