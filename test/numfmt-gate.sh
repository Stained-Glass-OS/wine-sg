#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Globalization.NumberFormatting (patches/sg/6199): DecimalFormatter formats with its
# options (integer, fraction and significant digits, grouping) and its language's separators, through
# its NumberRounder; it parses what it formats and gives null for other text; SignificantDigits- and
# IncrementNumberRounder round. WinUI 2's NumberBox (Windows Terminal's settings) makes them in its
# constructor; without them every NumberBox failed to be made.
#   WINE=/opt/wine-sg/bin/wine test/numfmt-gate.sh
# Mutants (dlls/windows.globalization): SG_MUTANT_NO_NUMBER_FORMATTING, SG_MUTANT_ROUNDER_TRUNCATES,
# SG_MUTANT_FORMATTER_LOCALE_IGNORED, SG_MUTANT_FORMATTER_NO_PARSER.
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

T=$(mktemp -d /var/tmp/sg-numfmt.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/numfmt-probe.exe" "$HERE/numfmt-probe.c" -lole32 -luuid -lruntimeobject \
    || { fail "probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
unset DISPLAY WAYLAND_DISPLAY
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/numfmt-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -q "^$1\$"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%[= ]*}" || echo none))"; fi
}

check 'defaults=1234.5|0.25|-3|0'                    "DecimalFormatter: plain numbers"
check 'options=1,234,567.50|007.00'                  "IsGrouped, FractionDigits 2, IntegerDigits 3"
check 'parse=1234.5|-0.75|null'                      "ParseDouble: grouped, negative; null for no number"
check 'significant=12300'                            "SignificantDigitsNumberRounder: 3 digits"
check 'significant_small=0.00123'                    "and of a small number"
check 'increment=1.25'                               "IncrementNumberRounder: to 0.25"
check 'rounded=98800'                                "a formatter's NumberRounder rounds first"
check 'sigdigits=0.5000'                             "SignificantDigits 4 shows trailing zeros"
check 'german=1.234,5|1234.5'                        "a German formatter: its separators, both ways"
check 'done=1'                                       "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
