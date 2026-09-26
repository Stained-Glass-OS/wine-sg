#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msvcp140's std::mutex and std::condition_variable in the layout Visual
# Studio 2022 17.10 and later construct inline (patches/sg/0428): the SRW
# lock after _Type and a pointer-sized slot, the condition variable after a
# pointer-sized slot -- as Microsoft's open-source STL (xthreads.h) lays them
# out. Programs built since then initialise these structures themselves and
# hand them to msvcp140. 64- and 32-bit.
#
#   WINE=/opt/wine-sg/bin/wine test/stlmtx-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-stlmtx.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$T/p64.exe" "$HERE/stlmtx-probe.c" &&
    i686-w64-mingw32-gcc -O2 -o "$T/p32.exe" "$HERE/stlmtx-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    out=$(timeout -s KILL 60 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    has() { printf '%s\n' "$out" | grep -qx "$1"; }
    has 'SRW_FREE 1' && has 'SRW_HELD 1' && has 'SRW_RELEASED 1' \
        && pass "$p: the mutex's SRW lock is where the STL puts it (held while locked)" || fail "$p: mutex layout"
    has 'CV_USED 1' && pass "$p: the condition variable is where the STL puts it (used by wait and signal)" || fail "$p: condition variable layout"
    has 'WAITER_DONE 1' && pass "$p: wait and signal still work" || fail "$p: wait/signal"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
