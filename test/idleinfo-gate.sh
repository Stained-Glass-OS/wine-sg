#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# NtQuerySystemInformation(SystemProcessorIdleInformation, 42) (patches/sg/9751). HWMonitor asks
# for it on every refresh; Wine had no such class ("NtQuerySystemInformation (0x0000002a,...) stub",
# STATUS_NOT_IMPLEMENTED). The probe (test/idleinfo-probe.c), 64-bit and 32-bit (HWMonitor is
# 32-bit: the class goes through WoW64), checks:
#   a buffer of 48 bytes per processor: STATUS_SUCCESS, that length returned
#   every processor's IdleTime is non-zero and agrees with SystemProcessorPerformanceInformation's
#   processor 0's C1 + C2 + C3 time is non-zero where the kernel has cpuidle states
#   one byte short: STATUS_INFO_LENGTH_MISMATCH with the length needed
#   WINE=/opt/wine-sg/bin/wine test/idleinfo-gate.sh
# Mutants (ntdll/unix/system.c): SG_MUTANT_NO_IDLE_INFO, SG_MUTANT_NO_CSTATES.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-idleinfo.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe64.exe" "$HERE/idleinfo-probe.c" &&
i686-w64-mingw32-gcc -O1 -o "$T/probe32.exe" "$HERE/idleinfo-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
CST=0; [ -r /sys/devices/system/cpu/cpu0/cpuidle/state1/time ] && CST=1
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
for b in 64 32; do
    out=$(timeout 120 "$WINE" "$T/probe$b.exe" 2>/dev/null | tr -d '\r')
    printf '      %s\n' $(printf '%s\n' "$out" | tr ' ' '=')
    v() { printf '%s\n' "$out" | awk -v k="$1" -v f="$2" '$1 == k { print $f; exit }'; }
    n=$(v CPUS 2); want=$(( ${n:-0} * 48 ))
    [ "$(v FULL 2)" = 00000000 ] && [ "$(v FULL 3)" = "$want" ] && pass "$b-bit: 48 bytes per processor, STATUS_SUCCESS" || fail "$b-bit: '$(v FULL 2) $(v FULL 3)' (want 00000000 $want)"
    [ "$(v IDLE 2)" = ok ] && pass "$b-bit: every processor's idle time, as the performance information has it" || fail "$b-bit: idle times: $(v IDLE 2) $(v IDLE 3)"
    if [ $CST = 1 ]; then
        [ "${n:-0}" -gt 0 ] && [ "$(v CSTATE 2)" != 0 ] && pass "$b-bit: C-state residency from the kernel's cpuidle" || fail "$b-bit: C-state time '$(v CSTATE 2)'"
    fi
    [ "$(v SHORT 2)" = c0000004 ] && [ "$(v SHORT 3)" = "$want" ] && pass "$b-bit: one byte short: STATUS_INFO_LENGTH_MISMATCH, the length needed" || fail "$b-bit: short: '$(v SHORT 2) $(v SHORT 3)'"
done
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
