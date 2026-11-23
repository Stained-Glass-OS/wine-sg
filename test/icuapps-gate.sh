#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# icu.dll beyond the .NET list (patches/sg/9800): the ICU functions Telegram Desktop (Qt) delay-imports
# from Windows' icu.dll -- full case mapping, UTF-8, language tags, collation strength, the default time
# zone, number formatting with attributes and field positions, date field positions, list, plural and
# relative-date formatters -- forwarded to the host's ICU like the rest (make_host.py, app_names.txt).
# Run 64- and 32-bit (the 32-bit side keeps ICU's objects as handles and widens ulistfmt_format's array
# of strings). An open with a default/fallback locale leaves its warning in the status (negative codes, as ICU
# does on Windows too). test/icuapps-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/icuapps-gate.sh
# Mutant: dlls/icu/app_names.txt emptied (make_host.py run again) -> the probe finds the names missing.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc missing"; exit 77; }
command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: i686-w64-mingw32-gcc missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-icuapps.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in 64 32; do
    cc=x86_64-w64-mingw32-gcc; [ $a = 32 ] && cc=i686-w64-mingw32-gcc
    TMPDIR=/var/tmp $cc -O2 -o "$T/icuapps$a.exe" "$HERE/icuapps-probe.c" || { echo "FAIL  the $a-bit probe did not build"; exit 1; }
done
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 120 "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T"/icuapps*.exe "$WINEPREFIX/drive_c/"
RC=0
for a in 64 32; do
    # the host ICU's default zone follows TZ, as Windows' icu.dll follows the system's time zone
    out=$(cd "$WINEPREFIX/drive_c" && TZ=Europe/Berlin timeout -s KILL 120 "$WINE" icuapps$a.exe 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    chk() { if printf '%s\n' "$out" | grep -qE "$1"; then printf 'PASS  %s: %s\n' $a "$2"; else printf 'FAIL  %s: %s\n' $a "$2"; RC=1; fi; }
    chk '^load=1$' "icu.dll exports all 27 functions"
    chk '^upper=53 54 52 41 53 53 45 err=0$' "u_strToUpper: German sharp s upper-cases to SS"
    chk '^lower_tr=131 78 err=0$' "u_strToLower: Turkish I lower-cases to dotless i"
    chk '^lower_en=69 78 err=0$' "u_strToLower: English I to i"
    chk '^upper_small=7 err=15$' "u_strToUpper into a short buffer: the length needed, U_BUFFER_OVERFLOW_ERROR"
    chk '^from8=e9 d83d de00 err=0$' "u_strFromUTF8, with a surrogate pair"
    chk '^to8=e2 82 ac f0 9f 98 80 err=0$' "u_strToUTF8"
    chk '^totag=en-US 5 err=0$' "uloc_toLanguageTag"
    chk '^fortag=de_DE 5 5 err=0$' "uloc_forLanguageTag (and the parsed length)"
    chk '^coll=-1 0 err=(0|-[0-9]+)$' "ucol_setStrength: a and A equal at primary strength only"
    chk '^tz=Europe/Berlin\|err=0$' "ucal_getDefaultTimeZone"
    chk '^num=1,234\.5\|err=(0|-[0-9]+)$' "unum_formatDouble"
    chk '^num_frac=1,234\.50\|err=0$' "unum_setAttribute (minimum fraction digits)"
    chk '^negprefix=m\|err=0$' "unum_setTextAttribute / unum_getTextAttribute (negative prefix)"
    chk '^num_neg=m3\.00\|err=0$' "the negative prefix is used"
    chk '^numfield=.*0:0-5 ' "unum_formatDoubleForFields + ufieldpositer_next: the integer part"
    chk '^numfield=.*1:6-8 ' "the fraction part"
    chk '^date=1970-02\|err=(0|-[0-9]+)$' "udat_formatForFields"
    chk '^datefield=.*1:0-4 ' "the year field"
    chk '^datefield=.*2:5-7 ' "the month field"
    chk '^list=apples, pears, and figs\|err=0$' "ulistfmt_format of three"
    chk '^list2=apples and pears\|err=0$' "ulistfmt_format of two"
    chk '^plural=one other other $' "uplrules_select: cardinals"
    chk '^ordinal=one two few other $' "uplrules_openForType ordinals"
    chk '^rel=yesterday\|err=(0|-[0-9]+)$' "ureldatefmt_format"
    chk '^relnum=1 day ago\|err=0$' "ureldatefmt_formatNumeric"
    chk '^relhours=in 3 hours\|err=0$' "ureldatefmt_format in the future"
    chk '^done=1$' "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
