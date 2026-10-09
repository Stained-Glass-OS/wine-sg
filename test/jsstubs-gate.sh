#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# JScript stubs (patches/sg/1653), 64- and 32-bit: test/jsstubs-probe.c is
# a script host running JScript in the ES5 mode pages and HTAs get --
# JSON.stringify with toJSON (Date's too), an array replacer and circular
# values (TypeError 5034); JSON.parse errors (SyntaxError); decodeURI of
# bad sequences (URIError); apply with a non-array (TypeError 5028) or
# null; Function.prototype(); calling arguments; String.localeCompare;
# ArrayBuffer.isView; GetObject("", class) and GetObject(, class). These
# were E_NOTIMPL stubs or failed with E_FAIL.
#
#   WINE=/opt/wine-sg/bin/wine test/jsstubs-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_TOJSON, SG_MUTANT_NO_PROP_LIST (jscript/json.c),
# SG_MUTANT_URI_E_FAIL (jscript/global.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-jsstubs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/jsstubs-probe.c" -lole32 -loleaut32 -luuid \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
