#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Application restart and recovery (patches/sg/1624), 64- and 32-bit:
# test/apprestart-probe.c. RegisterApplicationRestart and
# RegisterApplicationRecoveryCallback kept nothing,
# GetApplicationRestartSettings was E_NOTIMPL, and a crash neither ran the
# recovery callback nor started the program again (Office, browsers and
# editors register both to come back with their documents).
#
# Checks: the registrations (this process's and another's, 64 reading 64,
# 64 reading 32, 32 reading 32); a crash at once runs the recovery but does
# not restart; a crash after a minute restarts (the gate takes ~70 s).
#
#   WINE=/opt/wine-sg/bin/wine test/apprestart-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (kernelbase/debug.c): SG_MUTANT_NO_APP_RESTART,
# SG_MUTANT_RESTART_AT_ONCE, SG_MUTANT_NO_CRASH_RECOVERY.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-apprestart.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/apprestart-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/apprestart-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX" "$T/early" "$T/late"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
run() {
    printf "== %s\n" "$*"
    out=$(cd "$T" && timeout -s KILL 120 env DISPLAY= "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
}
run "$T/p64.exe"
run "$T/p64.exe" 'Z:'"$(printf '%s' "$T/p32.exe" | tr / '\\')"
run "$T/p32.exe"

echo "== a crash at once"
(cd "$T" && timeout -s KILL 120 env DISPLAY= "$WINE" "$T/p64.exe" crash 'Z:'"$(printf '%s' "$T/early" | tr / '\\')" 0 >/dev/null 2>&1 </dev/null)
sleep 3
[ -e "$T/early/recovered" ] && echo "PASS  the recovery callback ran" || { echo "FAIL  the recovery callback did not run"; RC=1; }
[ ! -e "$T/early/restarted" ] && echo "PASS  a program crashing within a minute is not restarted" ||
    { echo "FAIL  it was restarted"; RC=1; }

echo "== a crash after a minute"
(cd "$T" && timeout -s KILL 150 env DISPLAY= "$WINE" "$T/p64.exe" crash 'Z:'"$(printf '%s' "$T/late" | tr / '\\')" 61 >/dev/null 2>&1 </dev/null)
i=0; while [ ! -e "$T/late/restarted" ] && [ $i -lt 40 ]; do sleep 0.5; i=$((i + 1)); done
[ -e "$T/late/recovered" ] && echo "PASS  the recovery callback ran" || { echo "FAIL  the recovery callback did not run"; RC=1; }
[ -e "$T/late/restarted" ] && echo "PASS  it was started again with its command line" || { echo "FAIL  it was not restarted"; RC=1; }

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
