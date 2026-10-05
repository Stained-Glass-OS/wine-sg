#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# IXMLDOMElement::removeAttributeNode (patches/sg/0812) was a stub
# (E_NOTIMPL): Meedio removes each attribute of its theme files as it reads
# it, and stopped at start with "Failed to load resource file
# ...\default.resources : Not implemented". The attribute now leaves the
# element and comes back, its value kept; one not on the element (removed
# already, or another element's) is refused.
#
#   WINE=/opt/wine-sg/bin/wine test/msxml-removeattr-gate.sh   (mutant SG_MUTANT_MSXML_REMOVEATTRNODE_STUB)
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

T=$(mktemp -d /var/tmp/sg-msxmlrmattr.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/msxml-removeattr-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }

[ "$(v remove)" = 00000000 ] && [ "$(v returned-same)" = 1 ] && pass "removeAttributeNode: S_OK, the attribute given back" || fail "remove: $(v remove) same $(v returned-same)"
[ "$(v left)" = 1 ] && [ "$(v get-removed)" = 00000001 ] && pass "the element keeps its other attribute only (getAttribute of the removed one: S_FALSE)" || fail "left $(v left), get $(v get-removed)"
[ "$(v xml)" = '<white r="1"/>' ] && pass "and is written without it" || fail "xml: $(v xml)"
[ "$(v value-kept)" = 255 ] && pass "the removed attribute keeps its value" || fail "value: $(v value-kept)"
[ "$(v again)" = 80070057 ] && [ "$(v other)" = 80070057 ] && [ "$(v null)" = 80070057 ] \
    && pass "one removed already, another element's, or none: E_INVALIDARG" || fail "refusals: $(v again) $(v other) $(v null)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
