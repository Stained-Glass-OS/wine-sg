#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# user32 leaves no exception frame behind (wine-sg 6270). 6269's check whether an icon's module is
# ours returned from inside a __TRY: every thread that loaded an icon of one of our modules kept a
# dead frame on its exception list, and a later exception called a handler from a dead stack slot --
# Teams died of a stack overflow of nested exceptions. Loading icons from shell32 (LoadImage and
# ExtractIconEx) must leave the thread's exception list as it was, and a C++ exception thrown deep in
# the stack afterwards must be caught, 64- and 32-bit. test/tryframe-probe.cpp. tools/try-check.py
# (make lint) keeps return, goto and break out of __TRY bodies in the files we patch.
#   WINE=/opt/wine-sg/bin/wine test/tryframe-gate.sh
# Mutant: SG_MUTANT_TRY_RETURN (dlls/user32/cursoricon.c: the return inside the __TRY again).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-g++ i686-w64-mingw32-g++; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-tryframe.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-g++ -O1 -static -o "$T/p64.exe" "$HERE/tryframe-probe.cpp" -lshell32 &&
    TMPDIR=/var/tmp i686-w64-mingw32-g++ -O1 -static -o "$T/p32.exe" "$HERE/tryframe-probe.cpp" -lshell32 ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
        echo "ok   $p ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
    else
        echo "FAIL $p"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
    fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
