#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE netio.sys: the WSK client driver of Wine's own ntoskrnl test (driver_netio.c) -- transport
# addresses, send/receive flags, disconnect, ConnectEx, control, name info, socket kinds, error mapping.
# Runs ntoskrnl.exe_test.exe from a build tree and fails on any failure in driver_netio.c.
#
#   WINE=/path/to/build/wine test/ntoskrnl-netiotest-gate.sh   (SKIP without the built test)
# Mutants: listed in each patch's notes (netio.sys/netio.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
TEST="${NTOSKRNL_TEST:-$(dirname "$WINE")/dlls/ntoskrnl.exe/tests/x86_64-windows/ntoskrnl.exe_test.exe}"
[ -f "$TEST" ] || { echo "SKIP: no ntoskrnl.exe_test.exe at $TEST"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ntoskrnl-netio.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(cd "$T" && timeout -s KILL 280 "$WINE" "$TEST" 2>&1 </dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -q 'Subtest driver_netio' || { echo "FAIL the driver_netio subtest did not run"; printf '%s\n' "$out" | tail -5; echo "RESULT: FAIL"; exit 1; }
bad=$(printf '%s\n' "$out" | grep -E 'driver_netio\.c:[0-9]+: Test (failed|succeeded inside todo)')
n=$(printf '%s\n' "$out" | grep -c 'driver_netio\.c')
if [ -z "$bad" ]; then echo "ok   driver_netio ($n lines of todo / info from the driver)"; echo "RESULT: PASS"; exit 0; fi
printf 'FAIL %s\n' "$bad"; echo "RESULT: FAIL"; exit 1
