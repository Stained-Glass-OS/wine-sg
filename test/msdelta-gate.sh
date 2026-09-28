#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msdelta's delta API refuses with an error instead of killing the caller
# (patches/sg/0475).
#
# Every export was a bare stub: Microsoft 365's Click-to-Run service updates
# its own files from deltas, called ApplyDeltaB and was killed
# ("unimplemented function msdelta.dll.ApplyDeltaB, aborting") mid-install.
# Applying a delta is still not implemented; the calls now return FALSE with
# ERROR_NOT_SUPPORTED (50), so a caller falls back to the whole file. Both
# 64- and 32-bit (the 32-bit spec pops by-value DELTA_INPUTs).
#
#   WINE=/opt/wine-sg/bin/wine test/msdelta-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-msdelta.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    bits=64; [ "$cc" = i686-w64-mingw32-gcc ] && bits=32
    command -v "$cc" >/dev/null || { echo "info  $cc missing: $bits-bit skipped"; continue; }
    "$cc" -O2 -o "$T/probe$bits.exe" "$HERE/msdelta-probe.c" || { fail "$bits-bit probe did not build"; continue; }
    out=$(timeout -s KILL 60 "$WINE" "$T/probe$bits.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $bits: /"
    printf '%s\n' "$out" | grep -qx 'ApplyDeltaB 0 50 output-cleared' && pass "$bits-bit: ApplyDeltaB refuses (ERROR_NOT_SUPPORTED), the caller lives" \
        || fail "$bits-bit: ApplyDeltaB: $(printf '%s\n' "$out" | grep ApplyDeltaB || echo 'the caller was killed')"
    printf '%s\n' "$out" | grep -qx 'ApplyDeltaW 0 50' && printf '%s\n' "$out" | grep -qx 'GetDeltaInfoB 0 50' \
        && pass "$bits-bit: ApplyDeltaW and GetDeltaInfoB refuse too" || fail "$bits-bit: ApplyDeltaW/GetDeltaInfoB"
    printf '%s\n' "$out" | grep -qx 'stack kept' && pass "$bits-bit: the caller's stack is intact" || fail "$bits-bit: stack"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
