#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# kernel32's GetProcAddress goes through kernelbase (patches/sg/0644), as on
# Windows 8 and later. TWAINDSM.dll replaces ntdll!LdrGetProcedureAddress in
# kernelbase's import table so that a TWAIN 1.x data source's
# GetProcAddress( TWAIN_32, "DSM_Entry" ) reaches it; Wine's kernel32 had its
# own GetProcAddress, the data source got the old TWAIN_32.DLL that TWAINDSM
# had loaded without its imports, and the Ambir ImageScan Pro 490i's scan
# crashed at 0000B1F0. The probe hooks the slot the same way, 32- and 64-bit.
#
#   WINE=/opt/wine-sg/bin/wine test/gpahook-gate.sh
# Mutation: kernel32.spec without the forward (10.0-132): redirected=0.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null ||
    { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-gpahook.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for cc in x86_64 i686; do
    TMPDIR=/var/tmp $cc-w64-mingw32-gcc -O2 -o "$T/gpa-$cc.exe" "$HERE/gpahook-probe.c" || { fail "$cc probe did not build"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for cc in i686 x86_64; do
    out=$(timeout 60 "$WINE" "$T/gpa-$cc.exe" 2>/dev/null | tr -d '\r')
    v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
    [ "$(v slot)" = 1 ] || { fail "$cc: kernelbase imports no ntdll!LdrGetProcedureAddress: $out"; continue; }
    [ "$(v redirected)" = 1 ] && pass "$cc: kernel32 GetProcAddress answers from kernelbase's hooked import (TWAINDSM)" ||
        fail "$cc: kernel32 GetProcAddress bypassed kernelbase: $out"
    [ "$(v others)" = 1 ] && pass "$cc: other lookups unchanged" || fail "$cc: other lookups: $out"
done
exit $RC
