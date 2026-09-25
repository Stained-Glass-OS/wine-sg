#!/bin/sh
# The user's regional format is their choice (patches/sg/0168).
#
# Wine took the user's locale from the Unix locale the process started with
# (LC_MESSAGES) and, at every process start, rewrote Control Panel\International
# to match it -- so a format chosen in Settings > Region or in the first-run
# setup was undone by the next program. Now a valid LocaleName there decides
# the format (the display language still follows the Unix locale); the other
# format values are regenerated once for a new choice, and a country chosen
# separately (Geo\Nation) is kept.
#
#   WINE=/opt/wine-sg/bin/wine test/region-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-region.XXXXXX)
# The Unix locale says US English, as the image's does.
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER LANG=C.UTF-8 LC_ALL=C.UTF-8
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/region-probe.exe" "$HERE/region-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
probe() { timeout -s KILL 120 "$WINE" "$T/region-probe.exe" 2>/dev/null | tr -d '\r' | paste -sd' '; }
reg() { timeout -s KILL 120 "$WINE" reg "$@" >/dev/null 2>&1; }
intl='HKCU\Control Panel\International'

out=$(probe); echo "      unchosen: $out"
case "$out" in "LOCALE en-US LCID 0409 SHORTDATE M/d/yyyy"*) pass "with no choice, the Unix locale's format (en-US)" ;;
    *) fail "unchosen: $out" ;; esac

reg add "$intl" /v LocaleName /d en-GB /f
out=$(probe); echo "      en-GB chosen: $out"
case "$out" in "LOCALE en-GB LCID 0809 SHORTDATE dd/MM/yyyy DATE 25/09/2026"*) pass "a chosen LocaleName decides the user's format (en-GB: dd/MM/yyyy)" ;;
    *) fail "en-GB chosen: $out" ;; esac
out=$(probe)
case "$out" in "LOCALE en-GB"*) pass "and stays chosen for the next program" ;; *) fail "the next program: $out" ;; esac
v=$(timeout 60 "$WINE" reg query "$intl" /v sShortDate 2>/dev/null | tr -d '\r' | awk '/sShortDate/ { print $3 }')
[ "$v" = dd/MM/yyyy ] && pass "the format values in the registry follow the choice (sShortDate $v)" || fail "sShortDate '$v'"

reg add "$intl\\Geo" /v Nation /d 94 /f
reg add "$intl" /v LocaleName /d de-DE /f
out=$(probe); echo "      de-DE chosen, country 94: $out"
case "$out" in *"LOCALE de-DE"*"DECIMAL , GEO 94") pass "a new format (de-DE, decimal comma) keeps the country chosen apart from it (94)" ;;
    *) fail "de-DE with country 94: $out" ;; esac

reg add "$intl" /v LocaleName /d xx-NOTREAL /f
out=$(probe)
case "$out" in "LOCALE en-US"*) pass "a LocaleName that is no locale is ignored" ;; *) fail "bogus LocaleName: $out" ;; esac

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
