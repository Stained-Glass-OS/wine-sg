#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DirectWrite: a font collection as a font set (patches/sg/0503). Office's
# text layout asks the system collection for its font set; Wine returned
# E_NOTIMPL. The set holds every face of every family, and its entries
# answer family-name queries (they knew only full and PostScript names).
#
#   WINE=/opt/wine-sg/bin/wine test/dwfontset-gate.sh
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
T=$(mktemp -d /var/tmp/sg-dwset.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/dwfontset-probe.c" -ldwrite -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
[ "$(v getfontset)" = 00000000 ] && pass "the system collection gives its font set" || fail "GetFontSet: $(v getfontset)"
[ "$(v fonts)" = 1 ] && pass "the set holds the collection's faces" || fail "fonts: $(v fonts)"
[ "$(v familynames)" = "00000000 1 1" ] && pass "its entries answer family names (Tahoma among them)" || fail "family names: $(v familynames)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
