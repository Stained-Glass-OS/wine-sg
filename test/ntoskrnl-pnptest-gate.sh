#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE ntoskrnl: the PnP driver of Wine's own ntoskrnl test (driver_pnp.c) -- PoSetPowerState returns
# the previous state, PoRequestPowerIrp sends a power IRP and calls back, IoGetDeviceInterfaces,
# IoRegisterPlugPlayNotification, IoGetDeviceProperty. Runs the "driver_pnp" subtest of
# ntoskrnl.exe_test.exe from a build tree and fails on any failure in driver_pnp.c.
#
#   WINE=/path/to/build/wine test/ntoskrnl-pnptest-gate.sh   (SKIP without the built test)
# Mutants: SG_MUTANT_POWER_REQUEST, SG_MUTANT_POWER_PREVIOUS, SG_MUTANT_IFACES_EMPTY, SG_MUTANT_IFACES_ALL (pnp.c); the later PnP patches list theirs here.
# SG_MUTANT_NOTIFY_NONE, SG_MUTANT_NOTIFY_NO_EXISTING (pnp.c); SG_MUTANT_DEVPROP_DRIVERKEY (pnp.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
TEST="${NTOSKRNL_TEST:-$(dirname "$WINE")/dlls/ntoskrnl.exe/tests/x86_64-windows/ntoskrnl.exe_test.exe}"
[ -f "$TEST" ] || { echo "SKIP: no ntoskrnl.exe_test.exe at $TEST"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ntoskrnl-pnp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(cd "$T" && timeout -s KILL 280 "$WINE" "$TEST" 2>&1 </dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -q 'Subtest driver_pnp' || { echo "FAIL the driver_pnp subtest did not run"; printf '%s\n' "$out" | tail -5; echo "RESULT: FAIL"; exit 1; }
bad=$(printf '%s\n' "$out" | grep -E 'driver_pnp\.c:[0-9]+: Test (failed|succeeded inside todo)')
n=$(printf '%s\n' "$out" | grep -c 'driver_pnp\.c')
if [ -z "$bad" ]; then echo "ok   driver_pnp ($n lines of todo / info from the driver)"; echo "RESULT: PASS"; exit 0; fi
printf 'FAIL %s\n' "$bad"; echo "RESULT: FAIL"; exit 1
