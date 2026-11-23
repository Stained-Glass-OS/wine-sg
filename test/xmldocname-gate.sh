#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# IXMLDocument::createElement names the element it makes and IXMLElement::addChild puts it at its index
# (wine-sg 9600, msxml3 xmldoc.c, xmlelem.c):
# the tag name was ignored and an index other than 0 misplaced the child.
#   WINE=/opt/wine-sg/bin/wine test/xmldocname-gate.sh
# Mutant: SG_MUTANT_XMLDOC_NONAME (dlls/msxml3/xmldoc.c, xmlelem.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xmldocname.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/xmldocname-probe.c" -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 90 "$WINE" probe.exe >"$T/probe.out" 2>/dev/null </dev/null)
tr -d '\r' < "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
echo "RESULT: FAIL"
exit 1
