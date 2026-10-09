#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DirectWrite odds and ends (patches/sg/1692), 64- and 32-bit:
# test/dwritemisc-probe.c checks locality, font face reference sizes, times
# and equality, axis values, font set axis ranges and filtering, matching by
# axis values (the list in order of the match), expiration events and
# AnalyzeNumberSubstitution; with a variable font from the host (any one
# fontconfig lists), HasVariations. These were stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/wscatalog-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_VARIATIONS (with a variable font), SG_MUTANT_NO_EXPIRATION_EVENT
# (dwrite/font.c), SG_MUTANT_NO_NUMBER_SUBST (dwrite/analyzer.c).
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
T=$(mktemp -d /var/tmp/sg-dwritemisc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/dwritemisc-probe.c" -ldwrite -luuid \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
VF=""
for f in /usr/share/fonts/opentype/cantarell/Cantarell-VF.otf \
         $(command -v fc-list >/dev/null && fc-list --format '%{file} %{variable}\n' 2>/dev/null | sed -n 's/ True$//p' | head -3); do
    [ -f "$f" ] && { cp "$f" "$WINEPREFIX/drive_c/variable-font.${f##*.}" && VF="C:\\variable-font.${f##*.}"; break; }
done
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" $VF 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
