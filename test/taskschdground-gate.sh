#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# taskschd/schedsvc behaviours recorded by Wine's scheduler test (patches/sg/2439), 64- and 32-bit: invalid folder names,
# EnableTask on a task its XML disables, DeleteTask without a name, task XML root and empty Actions (test/taskschdground-probe.c).
#
#   WINE=/opt/wine-sg/bin/wine test/taskschdground-gate.sh
# Mutants: SG_MUTANT_TS_NAMES (taskschd/folder.c), SG_MUTANT_TS_FORCE (schedsvc/taskrun.c), SG_MUTANT_TS_ACTIONS (taskschd/task.c).
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
T=$(mktemp -d /var/tmp/sg-taskschdground.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/taskschdground-probe.c" -lole32 -loleaut32 -luuid \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
