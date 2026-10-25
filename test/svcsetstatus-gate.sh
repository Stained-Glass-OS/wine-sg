#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A service's own process reports its status whatever the service's security descriptor says
# (6108). CCleaner 7's installer gives its service a descriptor that grants SERVICE_SET_STATUS to
# nobody; Wine's dispatcher opened the service for that right through the descriptor, was refused,
# and the service never reported running (StartService: 1053). services.exe now lets the service's
# own process (the RPC caller's pid, which ncacn_np now reports) open it for SERVICE_SET_STATUS.
# test/svcsetstatus-probe.c installs itself as the service, sets that descriptor, starts it.
#   WINE=/opt/wine-sg/bin/wine test/svcsetstatus-gate.sh
# Mutant: SG_MUTANT_SET_STATUS_BY_SD (programs/services/rpc.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-svcsetstatus.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/svcsetstatus-probe.c" -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' test 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && exit 0
printf '%s\n' "$out" | grep -q '^RESULT:' || echo "RESULT: FAIL (the probe gave no result)"
exit 1
