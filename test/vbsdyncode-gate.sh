#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Eval, Execute, ExecuteGlobal and GetRef (patches/sg/1652), 64- and 32-bit:
# test/vbsdyncode.vbs under cscript runs code in the calling procedure's
# scope (its variables, return value, Me, arrays it declares), at the
# global level, nested, with syntax and runtime errors raised where On
# Error handles them; test/vbsdyncode-host.c is a script host that keeps a
# GetRef object, calls it later from outside the script as an event, sees a
# handler's error reported, and has the object refuse calls once the script
# is closed. These were E_NOTIMPL stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/vbsdyncode-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_EXECUTE_GLOBAL_SCOPE (vbscript/interp.c),
# SG_MUTANT_GETREF_NOTHING (vbscript/global.c),
# SG_MUTANT_COMPILE_ERROR_REPORTED (vbscript/compile.c).
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
T=$(mktemp -d /var/tmp/sg-vbsdyncode.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/host-$a.exe" "$HERE/vbsdyncode-host.c" -lole32 -loleaut32 -luuid \
        || { echo "FAIL  host did not build"; exit 1; }
done
cp "$HERE/vbsdyncode.vbs" "$T/"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
ZT='Z:'"$(printf '%s' "$T" | tr / '\\')"
for arch in 64 32; do
    if [ $arch = 32 ]; then cs='C:\windows\syswow64\cscript.exe'; host=host-i686.exe; else cs=cscript; host=host-x86_64.exe; fi
    echo "== $arch-bit cscript"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$cs" //nologo "$ZT\\vbsdyncode.vbs" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
    echo "== $arch-bit host"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/$host" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
