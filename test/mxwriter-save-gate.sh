#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Saving formatted XML through MSXML's SAX reader and MXXMLWriter, the VB
# interfaces (patches/sg/1480): Delphi's xmldoc does it (Meedio Configuration
# saving its settings: "This has not been implemented yet", David's report
# 20261007-231028-4796e1), as do scripts. Three faults: the writer's output
# set to a string ("write to a string") was E_NOTIMPL; the reader handed VB
# handlers a NULL namespace for an element in none, which the writer (as on
# Windows) refuses; and indenting put a line break and tabs inside an
# element's text (<b>x\r\n\t</b>: its value grew at every save) and a second
# line break after a comment. test/mxwriter-save-probe.js, for MSXML 3 and 6.
#
#   WINE=/opt/wine-sg/bin/wine test/mxwriter-save-gate.sh
# Mutants: SG_MUTANT_MXWRITER_NO_STRING_OUTPUT, SG_MUTANT_MXWRITER_INDENT_TEXT
# (mxwriter.c), SG_MUTANT_SAXREADER_VB_NULL (saxreader.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mxwriter.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
cp "$HERE/mxwriter-save-probe.js" "$T/probe.js"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
want='<?xml version="1.0" encoding="UTF-16" standalone="no"?>\n<a>\n	<b>x</b>\n	<!-- c -->\n</a>'
for v in 3.0 6.0; do
    out=$(timeout 120 "$WINE" cscript //nologo "$("$WINE" winepath -w "$T/probe.js" | tr -d '\r')" "$v" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | grep -qx "output = '': ok" && pass "MSXML $v: the writer's output set to a string" \
        || fail "MSXML $v: output = '': $(printf '%s\n' "$out" | grep "^output = ")"
    printf '%s\n' "$out" | grep -qx "parse: ok" && pass "MSXML $v: the reader feeds the writer through the VB interfaces" \
        || fail "MSXML $v: parse: $(printf '%s\n' "$out" | grep "^parse")"
    [ "$(printf '%s\n' "$out" | sed -n 's/^output: ok //p')" = "$want" ] \
        && pass "MSXML $v: indented as Windows writes it (no line break inside an element's text)" \
        || fail "MSXML $v: written: $(printf '%s\n' "$out" | sed -n 's/^output: //p')"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
