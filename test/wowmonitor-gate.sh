#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A 32-bit installer adds a 64-bit port monitor, its port and a printer of a 64-bit driver, and a
# 64-bit program prints through it (6115). CutePDF Writer's Setup (32-bit) does this with
# cpwmon64_v40.dll; on Windows the 64-bit spooler loads the monitor. Here the print provider runs in
# the caller: AddMonitor/AddPort failed (LoadLibrary 193), AddPrinter looked for the driver in the
# x86 environment, and a port that the monitor keeps to itself (not under the registry's Ports key)
# found no monitor when printing; and (6116) the monitor's job calls had no thread token, where the
# spooler impersonates the job's owner (CutePDF's Save As helper got no user: no PDF). test/wowmonitor-mon.c is the monitor, test/wowmonitor-probe.c the
# 32-bit installer and the 64-bit printer.
#   WINE=/opt/wine-sg/bin/wine test/wowmonitor-gate.sh
# Mutants: SG_MUTANT_SPL_NO_HOST, SG_MUTANT_SPL_JOB_IN_WOW64 (6118), SG_MUTANT_PORT_BY_REGISTRY_ONLY, SG_MUTANT_PORT_NO_IMPERSONATION (6116)
#          (dlls/localspl/provider.c),
#          SG_MUTANT_ADDPRINTER_NATIVE_DRIVER (dlls/winspool.drv/info.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $cc >/dev/null || { echo "SKIP: $cc missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wowmonitor.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -shared -o "$T/sgmon64.dll" "$HERE/wowmonitor-mon.c" || { echo "FAIL  monitor did not build"; exit 1; }
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/install32.exe" "$HERE/wowmonitor-probe.c" -lwinspool || { echo "FAIL  probe did not build"; exit 1; }
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/print64.exe" "$HERE/wowmonitor-probe.c" -lwinspool || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
mkdir -p "$WINEPREFIX/drive_c/setup"
cp "$T/sgmon64.dll" "$T/install32.exe" "$WINEPREFIX/drive_c/setup/"
cp "$T/print64.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c/setup" && timeout -s KILL 120 "$WINE" install32.exe install 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'MONITOR 0' && pass "a 32-bit installer adds the 64-bit monitor" || fail "AddMonitor"
[ -f "$WINEPREFIX/drive_c/windows/system32/sgmon64.dll" ] && pass "the monitor is in system32, where the spooler loads it" || fail "no sgmon64.dll in system32"
printf '%s\n' "$out" | grep -qx 'PORT 0' && pass "it adds the monitor's port" || fail "AddPort"
printf '%s\n' "$out" | grep -qx 'PRINTER 0' && pass "it adds a printer of the 64-bit driver on that port" || fail "AddPrinter"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" print64.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^PRINT 1' && pass "a 64-bit program's job to the monitor's own port ends" || fail "print: $out"
grep -q SGMON-DATA "$WINEPREFIX/drive_c/sgmon-out.bin" 2>/dev/null && pass "the monitor got the job's data" || fail "sgmon-out.bin: $(cat "$WINEPREFIX/drive_c/sgmon-out.bin" 2>/dev/null | head -c 40)"
grep -q 'TOKEN=1' "$WINEPREFIX/drive_c/sgmon-out.bin" 2>/dev/null && pass "the monitor runs as the job's owner (its thread has a token, as under the spooler's impersonation)" || fail "no thread token in the monitor"
# (6118) a 32-bit program's job to that port: its spooler cannot load the 64-bit monitor; the
# job goes to the 64-bit print host, which spools and prints it (CutePDF Writer from 32-bit apps)
rm -f "$WINEPREFIX/drive_c/sgmon-out.bin"
cp "$T/install32.exe" "$WINEPREFIX/drive_c/print32.exe"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" print32.exe print 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^PRINT 1' && pass "a 32-bit program's job to the 64-bit monitor's port ends" || fail "print32: $out"
grep -q SGMON-DATA "$WINEPREFIX/drive_c/sgmon-out.bin" 2>/dev/null && pass "the monitor got the 32-bit program's data (through the print host)" || fail "32-bit job: sgmon-out.bin: $(head -c 40 "$WINEPREFIX/drive_c/sgmon-out.bin" 2>/dev/null)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
