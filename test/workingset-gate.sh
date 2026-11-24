#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# QueryWorkingSet / NtQueryVirtualMemory(MemoryWorkingSetInformation) (patches/sg/9758). Firefox and
# Thunderbird ask for their working set (16 bytes first, for the count); Wine had no such class
# ("NtQueryVirtualMemory ... info_class=1 ... Unknown information class"). The probe
# (test/workingset-probe.c), 64-bit and 32-bit, checks:
#   a 16-byte buffer fails with ERROR_BAD_LENGTH and gives the number of entries
#   a buffer for them all succeeds
#   a written global's page is there, read/write (4); main()'s page is there, execute/read (3), shared
#   a committed page never touched is not there; after a write it is, read/write
#   WINE=/opt/wine-sg/bin/wine test/workingset-gate.sh
# Mutant (ntdll/unix/virtual.c): SG_MUTANT_NO_WORKING_SET.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-workingset.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe64.exe" "$HERE/workingset-probe.c" -lpsapi &&
i686-w64-mingw32-gcc -O1 -o "$T/probe32.exe" "$HERE/workingset-probe.c" -lpsapi || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
for b in 64 32; do
    out=$(timeout 120 "$WINE" "$T/probe$b.exe" 2>/dev/null | tr -d '\r')
    printf '      %s\n' $(printf '%s\n' "$out" | tr ' ' '=')
    l() { printf '%s\n' "$out" | sed -n "s/^$1 //p" | head -1; }
    set -- $(l SMALL); [ "${1:-}" = fail ] && [ "${2:-}" = 24 ] && [ "${3:-0}" -gt 10 ] && pass "$b-bit: 16 bytes: ERROR_BAD_LENGTH and the count ($3)" || fail "$b-bit: small '$(l SMALL)'"
    set -- $(l FULL); [ "${1:-}" = ok ] && [ "${2:-0}" -gt 10 ] && pass "$b-bit: the whole working set ($2 pages)" || fail "$b-bit: full '$(l FULL)'"
    [ "$(l DATA)" = 4 ] && pass "$b-bit: a written global's page, read/write" || fail "$b-bit: data '$(l DATA)'"
    [ "$(l CODE)" = "3 1" ] && pass "$b-bit: main()'s page, execute/read, shared" || fail "$b-bit: code '$(l CODE)'"
    [ "$(l UNTOUCHED)" = absent ] && [ "$(l TOUCHED)" = 4 ] && pass "$b-bit: a committed page joins it when written" || fail "$b-bit: untouched '$(l UNTOUCHED)', touched '$(l TOUCHED)'"
done
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
