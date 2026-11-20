#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# JScript's __defineGetter__ / __defineSetter__ take a host's function object as
# the accessor (patches/sg/8220): test/jshostacc-probe.c, 64- and 32-bit. They
# failed with E_NOTIMPL ("getter is not JS object").
#
#   WINE=/opt/wine-sg/bin/wine test/jshostacc-gate.sh
# Mutant: SG_MUTANT_NO_DISP_ACCESSOR (jscript/function.c, object.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for c in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v "$c" >/dev/null || { echo "SKIP: $c not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-jshostacc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/jshostacc-probe.c" -lole32 -loleaut32 -luuid &&
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/jshostacc-probe.c" -lole32 -loleaut32 -luuid ||
    { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for b in p64 p32; do
    timeout -s KILL 120 "$WINE" "$T/$b.exe" 2>/dev/null </dev/null | tr -d '\r' > "$T/$b.out"
    sed 's/^/      /' "$T/$b.out"
    grep -q '^DONE' "$T/$b.out" || { fail "$b: the probe did not finish ($(tail -1 "$T/$b.out"))"; continue; }
    for k in define getter setter descriptor; do
        grep -q "^$k=1" "$T/$b.out" && pass "$b: $k" || fail "$b: $k ($(grep "^$k=" "$T/$b.out"))"
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
