#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DECISIONS-2 NET-META: the metadata writer (IMetaDataEmit/Emit2/AssemblyEmit over DefineScope and ofWrite): types,
# members defined out of order, constants, layout, properties, events, pinvoke, attributes; read back live (with
# pointer tables) and after Save (renumbered, sorted, IMapToken told), 64/32 bit.
#
#   WINE=/opt/wine-sg/bin/wine test/fastnet-emit1-gate.sh      (ARCHS="x86_64" for one architecture)
# Mutants (rometadata/emit.c): SG_MUTANT_EMIT_NOSORT, _NOPACK, _NOREMAP, _NOMAP, _PTRLIVE, _CONST, _DUP, _NESTED, _FLAGS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
ARCHS="${ARCHS:-x86_64 i686}"
for a in $ARCHS; do
    command -v $a-w64-mingw32-gcc >/dev/null || { echo "SKIP: $a-w64-mingw32-gcc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-netemit1.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in $ARCHS; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/fastnet-emit1-probe.c" -lole32 -luuid \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in $ARCHS; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
