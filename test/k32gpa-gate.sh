#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# kernel32's GetProcAddress is its own export (patches/sg/6544). Wine's kernel32 forwarded
# GetProcAddress to kernelbase; Windows' exports it as code, as it does LoadLibrary* and
# GetModuleHandle*. PotPlayer's packer reads kernel32's export table for it, could not follow
# the forwarder and died at start (a stack overflow). test/k32gpa-probe.c, 64- and 32-bit.
#   WINE=/opt/wine-sg/bin/wine test/k32gpa-gate.sh
# Mutant: a .spec has no preprocessor; the mutant is the old line
# ("@ stdcall GetProcAddress(long str) kernelbase.GetProcAddress" in dlls/kernel32/kernel32.spec).
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
T=$(mktemp -d /var/tmp/sg-k32gpa.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/k32gpa-probe.c" || { fail "$a probe did not build"; exit 1; }
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
    check all_code         "kernel32 exports GetProcAddress, LoadLibrary* and GetModuleHandle* as code"
    check gpa_works        "kernel32's GetProcAddress, called from its export table, finds a function"
    check gpa_missing_null "and fails a missing one with ERROR_PROC_NOT_FOUND"
    check done             "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
