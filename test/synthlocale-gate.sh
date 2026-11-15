#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A locale name Windows has no data of its own for (wine-sg 7785). Windows 10 answers
# GetLocaleInfoEx for any well-formed name (language, script, region) as a "custom unspecified"
# locale, LCID 0x1000: the name in canonical case, its language's data. .NET 5+ on Windows
# create an ICU culture only when GetLocaleInfoEx(name, LOCALE_SNAME) succeeds: CultureInfo
# ("sr-SP") threw and DevToys 2 died at its start. test/synthlocale-probe.c asks for known,
# unknown and malformed names.
#   WINE=/opt/wine-sg/bin/wine test/synthlocale-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-synthlocale.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/synthlocale-probe.c" \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
expect() {
    if printf '%s\n' "$out" | grep -qxF "$1"; then pass "$2"
    else fail "$2: wanted '$1', got: $(printf '%s\n' "$out" | grep "^${1%%=*}=" | head -1)"; fi
}
expect "en-US=en-US len=6 lcid=0409 lang=English" "a locale of its own is as before"
expect "sr-SP=sr-SP len=6 lcid=1000 lang=Serbian" "sr-SP: a custom locale, LCID 0x1000, Serbian's data"
expect "xx-YY=xx-YY len=6 lcid=1000 lang=Invariant Language" "xx-YY: an unknown language, the invariant locale's data"
expect "en-XX=en-XX len=6 lcid=1000 lang=English" "en-XX: an unknown region of a known language"
expect "SR-sp=sr-SP len=6 lcid=1000 lang=Serbian" "the name comes back in canonical case"
expect "zh-hans-xx=zh-Hans-XX len=11 lcid=1000 lang=Chinese (Simplified)" "language, script and region (zh-Hans's data)"
expect "de-419=de-419 len=7 lcid=1000 lang=German" "a region of three digits"
for n in dummy e en- en-US- 1a-BB en-ABCD-EF-GH; do expect "$n=fail 87" "malformed '$n' still fails (ERROR_INVALID_PARAMETER)"; done
expect "en_XX=en-XX len=6 lcid=1000 lang=English" "an underscore separates as a hyphen does"
expect "short=0 122" "a short buffer gives ERROR_INSUFFICIENT_BUFFER"
expect "size=6" "the size is asked for with no buffer"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
