#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Resources of a language-neutral module from its MUI file (patches/sg/8411), 64- and 32-bit:
# test/muires-probe.c with modules built from test/muires-*.rc. Notepad3 loads lng\np3lng.dll
# (only a "MUI" resource) as a data file and exited when the strings and dialogs of
# lng\<language>\np3lng.dll.mui were not found.
#
#   WINE=/opt/wine-sg/bin/wine test/muires-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_MUI_RESOURCES, SG_MUTANT_MUI_LOADRESOURCE_FROM_MODULE (kernelbase/loader.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc x86_64-w64-mingw32-windres i686-w64-mingw32-windres; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-muires.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    D="$T/$a"; mkdir -p "$D"
    for r in ln sat plain; do
        TMPDIR=/var/tmp $a-w64-mingw32-windres "$HERE/muires-$r.rc" "$D/$r.o" || { echo "FAIL  windres $a $r"; exit 1; }
    done
    { TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$D/probe.exe" "$HERE/muires-probe.c" &&
      TMPDIR=/var/tmp $a-w64-mingw32-gcc -shared -o "$D/ln.dll" "$HERE/muires-stub.c" "$D/ln.o" &&
      cp "$D/ln.dll" "$D/ln2.dll" &&
      TMPDIR=/var/tmp $a-w64-mingw32-gcc -shared -o "$D/plain.dll" "$HERE/muires-stub.c" "$D/plain.o" &&
      TMPDIR=/var/tmp $a-w64-mingw32-gcc -shared -o "$D/sat.mui" "$HERE/muires-stub.c" "$D/sat.o"; } ||
        { echo "FAIL  $a probe or modules did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T/$a" && timeout -s KILL 120 "$WINE" "$T/$a/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
