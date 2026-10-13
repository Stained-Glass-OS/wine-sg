#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The C threads layer (patches/sg/4532), 64- and 32-bit: _Mtxinit/_Mtxlock/
# _Mtxunlock/_Mtxdst (recursive, contended), _Mtx_timedlock (time out and
# success), _Thrd_exit, _Thrd_abort, _Tss_* (per thread values, destructors
# at thread exit, none after delete) and _Once (four threads) in msvcp100 ..
# msvcp140, msvcp_win: test/msvcp-threads-c-probe.c. All were spec stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/msvcp-threads-c-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_TIMEDLOCK_NO_WAIT and SG_MUTANT_TSS_SET_NOP (msvcp90/misc.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msvcp-threads-c.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/msvcp-threads-c-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/msvcp-threads-c-probe.c" ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    for dll in msvcp100 msvcp110 msvcp120 msvcp140 msvcp_win; do
        out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" $dll 2>/dev/null </dev/null | tr -d '\r')
        if printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then
            echo "ok   $p $dll ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
        else
            echo "FAIL $p $dll"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
        fi
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
