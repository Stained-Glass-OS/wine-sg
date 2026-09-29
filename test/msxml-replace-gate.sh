#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# removeChild of a node replaceChild took out of the same parent
# (patches/sg/0490). Office's installer merges App-V manifests: it replaces
# an Extensions element, then removes the element it replaced from the same
# parent -- which failed with E_INVALIDARG, and Office stopped with
# 30088-45. Now that removal succeeds, to no effect; every other removal of a
# node that is not a child is still refused, as MSXML refuses it (a node
# removed already, one asked of another parent, one moved elsewhere).
#
#   WINE=/opt/wine-sg/bin/wine test/msxml-replace-gate.sh
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

T=$(mktemp -d /var/tmp/sg-msxmlrep.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/msxml-replace-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }

[ "$(v replace)" = 00000000 ] && pass "replaceChild takes a child out" || fail "replace: $(v replace)"
[ "$(v remove-replaced)" = 00000000 ] && [ "$(v remove-replaced-out)" = 1 ] \
    && pass "removeChild of the node it replaced, from the same parent: S_OK, the node back" || fail "remove replaced: $(v remove-replaced) out $(v remove-replaced-out)"
[ "$(v children)" = 2 ] && pass "and to no effect: the parent keeps its two children" || fail "children: $(v children)"
[ "$(v remove-from-other)" = 80070057 ] && pass "asked of another parent: E_INVALIDARG" || fail "other parent: $(v remove-from-other)"
[ "$(v remove-child)" = 00000000 ] && [ "$(v remove-removed)" = 80070057 ] \
    && pass "a child removed already is refused a second time (E_INVALIDARG, as MSXML)" || fail "removed twice: $(v remove-child) $(v remove-removed)"
[ "$(v remove-moved)" = 80070057 ] && pass "a replaced node moved into another parent: E_INVALIDARG" || fail "moved: $(v remove-moved)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
