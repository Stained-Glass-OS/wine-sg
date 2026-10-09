#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ShutdownBlockReasonCreate/Destroy/Query (patches/sg/2202): test/blockreason-probe.c
# sets, reads back, replaces and clears a window's shutdown-block reason.
# Create and Destroy were "stub" ERROR_CALL_NOT_IMPLEMENTED and Query was
# not even exported; a window never had one.
#
#   WINE=/opt/wine-sg/bin/wine test/blockreason-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# The probe also queries from a second process (a 64-bit and a 32-bit one
# started by each probe): the reason is server-side, not a pointer.
# Mutants (user32/user_main.c): SG_MUTANT_NO_REASON_CLEAR, SG_MUTANT_NO_OWNER_CHECK.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
command -v Xvfb >/dev/null || { echo "SKIP: needs Xvfb"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-blockreason.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
DISP=:224
Xvfb "$DISP" -screen 0 1024x768x24 >"$T/xvfb.log" 2>&1 &
XPID=$!
cleanup() { "$WINESERVER" -k 2>/dev/null; kill "$XPID" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/blockreason-probe.c" -luser32 -lkernel32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY="$DISP" "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for a in x86_64 i686; do
    o=$([ $a = x86_64 ] && echo i686 || echo x86_64)
    echo "== $a (other process: $o)"
    out=$(cd "$T" && timeout -s KILL 240 env DISPLAY="$DISP" "$WINE" "$T/probe-$a.exe" "$T/probe-$o.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
