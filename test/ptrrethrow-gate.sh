#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# std::rethrow_exception (patches/sg/1530): Windows throws a copy of the
# object the exception_ptr holds, so the catch that ends destroys only the
# copy. Wine threw the stored object; the catch destroyed it under the
# exception_ptr and OneNote went on to use it. 64-bit (hand-built throw info).
#
#   WINE=/opt/wine-sg/bin/wine test/ptrrethrow-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ptrrethrow.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$T/p64.exe" "$HERE/ptrrethrow-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in 64; do
    O=$(timeout 60 "$WINE" "$T/p$a.exe" 2>/dev/null | tr -d '\r')
    echo "      $a: $O"
    case "$O" in "copies=2 thrown=copy "*) pass "$a-bit: the rethrown object is a copy" ;; *) fail "$a-bit rethrow: $O" ;; esac
    case "$O" in *" stored=ok") pass "$a-bit: the exception_ptr's object is untouched" ;; *) fail "$a-bit stored: $O" ;; esac
done
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
