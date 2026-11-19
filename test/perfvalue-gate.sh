#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# PerfSet/Increment/Decrement{ULong,ULongLong}CounterValue (patches/sg/7858): advapi32 and
# kernelbase export them. Visual Studio 2022's C++ project system calls PerfSetULongCounterValue
# when it creates a project, and Wine ended devenv at "unimplemented function". Values land in the
# instance's counter data; a PERF_ATTRIB_BY_REFERENCE counter's go to the variable
# PerfSetCounterRefValue named. test/perfvalue-probe.c, 64- and 32-bit.
#   WINE=/opt/wine-sg/bin/wine test/perfvalue-gate.sh
# Mutant: -DSG_MUTANT_PERF_VALUE_NOT_BY_REFERENCE (dlls/kernelbase/main.c) writes a by-reference
# counter's value into the instance instead of its variable.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-perfvalue.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/perfvalue-probe.c" || { fail "$a probe did not build"; exit 1; }
done
unset DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
for a in x86_64 i686; do
    cp "$T/probe-$a.exe" "$WINEPREFIX/drive_c/"
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "C:\\probe-$a.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $a: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "(^| )$1=1( |$)"; then pass "$a: $2"
        else fail "$a: $2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
    }
    check exports   "advapi32 exports the six Perf*CounterValue functions"
    check ulong     "a ULONG counter is set, incremented and decremented in the instance"
    check ulonglong "a ULONGLONG counter likewise, past 32 bits"
    check byref     "a by-reference counter's value goes to its variable"
    check notfound  "an unknown counter id is ERROR_NOT_FOUND"
    check nullinst  "no instance is ERROR_INVALID_PARAMETER"
    check done             "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
