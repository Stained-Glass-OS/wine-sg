#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# icu.dll UText and regular expressions (patches/sg/6179): a UText provider of the program's giving its
# text a row at a time (Windows Terminal's buffer) searched with uregex_open/setUText/findNext/start64/
# end64 -- Windows Terminal's URL pattern across rows; ICU's common syntax and flags (LITERAL,
# CASE_INSENSITIVE, MULTILINE, DOTALL), groups, lazy and greedy quantifiers, intervals, classes;
# uregex_setText; syntax errors; utext_setup/close of a stack and a heap UText, nothing written past
# ICU's 144-byte UText. test/icuregex-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/icuregex-gate.sh
# Mutants (dlls/icu/uregex.c): SG_MUTANT_UREGEX_NOT_COMPILED, SG_MUTANT_UREGEX_NEVER_MATCHES,
# SG_MUTANT_UTEXT_PRIVB_64.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-icuregex.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/icuregex-probe.exe" "$HERE/icuregex-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/icuregex-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" icuregex-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=1$' "icu.dll has UText and the regular expressions Windows Terminal uses"
chk '^guard=144,1$' "utext_setup on a stack UText: ICU's 144 bytes, nothing written past them"
chk '^setup=1,0,4$' "utext_setup opens the program's UText"
chk '^urls=4-20 40-55,0$' "Windows Terminal's URL pattern across rows of a chunked UText"
chk '^literal=none,0$' "LITERAL: case kept"
chk '^nocase=26-29,0$' "LITERAL | CASE_INSENSITIVE"
chk '^words=0-3 22-25 36-39 50-53,0$' "word boundaries, classes, an interval"
chk '^bol=none,0$' "^ in MULTILINE without line ends"
chk '^alt=2-5 9-12,0$' "alternation"
chk '^groups=1-5/1/3 6-8/6/7,0$' "capture groups"
chk '^lazy=0-3 3-7,0$' "lazy quantifier"
chk '^greedy=0-7,0$' "greedy quantifier"
chk '^interval=2-4 5-8 10-13 13-15,0$' "{m,n}"
chk '^negclass=0-2 4-5 6-7,0$' "negated class with an escape"
chk '^dot=4-7,0$' ". stops at line ends"
chk '^dotall=0-3 4-7,0$' "DOTALL"
chk '^eol=2-3,0$' "$ before the end"
chk '^eolm=0-1 2-3,0$' "$ in MULTILINE"
chk '^empty=0-0 1-1 2-2,0$' "empty matches advance"
chk '^escape=0-3,0$' "\\x \\u and escaped symbols"
chk '^paren=1,10306$' "a missing ): U_REGEX_MISMATCHED_PAREN"
chk '^bracket=1,1030f$' "a missing ]: U_REGEX_MISSING_CLOSE_BRACKET"
chk '^interval_bad=1,10309$' "{3,1}: U_REGEX_MAX_LT_MIN"
chk '^null=1$' "a NULL expression: U_ILLEGAL_ARGUMENT_ERROR"
chk '^failed_before=1,1$' "an error before: nothing done"
chk '^close=1,1$' "utext_close calls the provider's close"
chk '^heap=1,0,7$' "utext_setup without a UText: one of its own, with extra space"
chk '^heap_close=1$' "and closing it frees it"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
