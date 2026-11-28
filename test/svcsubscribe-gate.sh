#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Service change subscriptions and a service whose process ends while it starts
# (patches/sg/9629, 9630; i0977). WireGuard for Windows' manager service called
# sechost's SubscribeServiceChangeNotifications (a stub: the process ended), after it had
# reported START_PENDING, and services.exe kept it "starting" with no process for ever:
# its installer waited for it. test/svcsubscribe-probe.c is its own test service.
# About 30 s per architecture.
#
#   WINE=/opt/wine-sg/bin/wine test/svcsubscribe-gate.sh
# Mutants: SG_MUTANT_EXIT_RUNNING_ONLY (programs/services/services.c),
# SG_MUTANT_NO_DATABASE_NOTIFY (programs/services/rpc.c), SG_MUTANT_SUBSCRIBE_STUB,
# SG_MUTANT_SUBSCRIBE_SHARED_HANDLE (dlls/sechost/service.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
WINESERVER=$(sg_wineserver); export WINESERVER
T=$(mktemp -d /var/tmp/sg-svcsubscribe.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
sg_gate_display || exit 77
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/svcsubscribe-probe.c" -ladvapi32 -lkernel32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 60 "$WINESERVER" -w
RC=0
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
