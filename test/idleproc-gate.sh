#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SystemProcessInformation lists the System Idle Process, and every process's cycle time, as
# Windows does (wine-sg 8402). Process id 0 comes first, with no name, a thread per processor,
# the processors' idle time and idle cycles; each process's CycleTime is its CPU time in TSC
# cycles (QueryProcessCycleTime's measure). Cycle-based CPU usage (System Informer, Process
# Explorer) is each process's cycle delta over the sum of all of them, the idle process's
# included: without it System Informer showed the total as 100% or -1604%. EnumProcesses and
# toolhelp list process 0 too.
# test/idleproc-probe.c, 64- and 32-bit: process 0 first; its threads and times; the probe's own
# cycle time; EnumProcesses/toolhelp; usage over a second with one thread busy is in 0..100% and
# the busy process and the idle one both have a share.
#   WINE=/opt/wine-sg/bin/wine test/idleproc-gate.sh
# Mutants: -DSG_MUTANT_NO_IDLE_PROCESS, -DSG_MUTANT_NO_PROCESS_CYCLES (dlls/ntdll/unix/system.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-idleproc.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/idleproc-probe.c" -lntdll -lpsapi || { fail "$a probe did not build"; exit 1; }
done
unset DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for a in x86_64 i686; do
    cp "$T/probe-$a.exe" "$WINEPREFIX/drive_c/"
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "C:\\probe-$a.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $a: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "^$1=1"; then pass "$a: $2"
        else fail "$a: $2 ($(printf '%s\n' "$out" | grep "^$1=" | head -1 || echo none))"; fi
    }
    check idle          "process 0 comes first, with no name (the System Idle Process)"
    check idlethreads   "with a thread per processor"
    check idletimes     "with the processors' idle time and idle cycles"
    check owncycles     "a process's CycleTime is its CPU time in cycles"
    check enumprocesses "EnumProcesses lists process 0"
    check toolhelp      "toolhelp lists process 0"
    check usage         "cycle-based usage over a second: in 0..100%, idle and the busy process have shares"
    check done          "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
