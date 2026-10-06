#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A wineserver that aborts says where (1102). glibc aborts a process whose
# heap it finds corrupt ("malloc(): unaligned tcache chunk detected"); Wine
# turns SIGABRT into exit(1), so no core dump was written and the machine's
# server, dying on a live medium (s14 regression walk, 2026-10-06), left one
# line in the journal. Now the call stack goes to stderr first, and the
# server still ends as before (exit(1): its atexit work, the registry, runs).
# A scratch prefix's server is sent SIGABRT:
#   1. its stderr has "aborted (SIGABRT)" and at least three frames of
#      wineserver's own
#   2. it exits with status 1, as before
#
#   WINE=/opt/wine-sg/bin/wine test/srvabort-gate.sh
#   (mutant SG_MUTANT_ABORT_SILENT in server/main.c)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINE" ] && [ -x "$WINESERVER" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-srvabort.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINESERVER" -f -p > "$T/out" 2>&1 & SP=$!
i=0; while [ ! -S "$(ls -d /tmp/.wine-$(id -u)/server-*/socket 2>/dev/null | head -1)" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
sleep 1
kill -ABRT "$SP"
wait "$SP"; st=$?
frames=$(grep -c 'wineserver(' "$T/out")
grep -q 'aborted (SIGABRT)' "$T/out" && [ "$frames" -ge 3 ] && pass "the call stack is written ($frames frames)" \
    || fail "no call stack: $(head -3 "$T/out" | tr '\n' '|')"
[ "$st" = 1 ] && pass "it ends with status 1, as before" || fail "exit status $st"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
