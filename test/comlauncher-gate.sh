#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A COM local server started through a launcher (patches/sg/0628). The program
# a class names may start the real server and exit at once: Omaha's
# BraveUpdateOnDemand.exe starts "BraveUpdate.exe /ondemand", which registers
# the class. COM took the launcher's exit for the server's failure and gave up
# within a second, so Brave's installer never opened the browser it had
# installed ("no class object ... could be created"). Now a program that exits
# successfully leaves COM waiting for the class; one that fails (exit code 3,
# nothing registered) still fails at once. 64-bit and 32-bit (WoW64).
#
#   WINE=/opt/wine-sg/bin/wine test/comlauncher-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for a in x86_64 i686; do
    command -v "$a-w64-mingw32-gcc" >/dev/null || { echo "SKIP: $a mingw-w64 not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-comlauncher.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp "$a-w64-mingw32-gcc" -O2 -o "$T/cl-$a.exe" "$HERE/comlauncher-probe.c" -lole32 -luuid ||
        { fail "probe did not build ($a)"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T"/cl-*.exe "$WINEPREFIX/drive_c/"
for a in x86_64 i686; do
    timeout 60 "$WINE" "C:\\cl-$a.exe" register >/dev/null 2>&1
    out=$(timeout 120 "$WINE" "C:\\cl-$a.exe" client 2>/dev/null | tr -d '\r')
    set -- $(printf '%s\n' "$out" | sed -n 's/^launched=//p')
    [ "${1:-}" = 00000000 ] && pass "$a: a class whose program starts the server and leaves is created (${2:-?} ms)" ||
        fail "$a: launched ${1:-none} after ${2:-?} ms"
    set -- $(printf '%s\n' "$out" | sed -n 's/^broken=//p')
    [ -n "${1:-}" ] && [ "$1" != 00000000 ] && [ "${2:-99999}" -lt 5000 ] &&
        pass "$a: a server that fails still fails at once ($1, $2 ms)" ||
        fail "$a: broken ${1:-none} after ${2:-?} ms"
    "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
done
exit $RC
