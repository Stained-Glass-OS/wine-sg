#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Concurrency Runtime details (patches/sg/4506), 64- and 32-bit:
# _Concurrent_queue_base_v4::_Internal_swap (concrt140, msvcp110, msvcp120) on
# queues of ints made with the probe's own item functions (a table of contents
# to swap, self swap); _Task_impl_base::_IsNonBlockingThread (msvcp140,
# msvcp_win) is false outside an application STA (no COM, desktop STA, MTA);
# _ExceptionHolder::ReportUnhandledError (msvcp140) ends the program through
# terminate() (exit code 3). All three were FIXME stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/crt-concurrency-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_QUEUE_SWAP_NOP, SG_MUTANT_NONBLOCKING_ANY_STA
# (concrt140/details.c, msvcp90/details.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-crt-conc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/crt-concurrency-probe.c" -lole32 &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/crt-concurrency-probe.c" -lole32 ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
run() {
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$1.exe" "$2" "$3" 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then echo "ok   $1 $2 $3 ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
    else echo "FAIL $1 $2 $3"; printf '%s\n' "$out" | grep -v '^ok'; RC=1; fi
}
for p in p64 p32; do
    for dll in concrt140 msvcp110 msvcp120; do run $p swap $dll; done
    for dll in msvcp140 msvcp_win; do run $p nonblocking $dll; done
    (cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" report msvcp140 >"$T/out.txt" 2>/dev/null </dev/null)
    status=$?
    if [ "$status" = 3 ] && ! grep -q RETURNED "$T/out.txt"; then echo "ok   $p report msvcp140 terminated (exit code 3)"
    else echo "FAIL $p report msvcp140: exit code $status"; RC=1; fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
