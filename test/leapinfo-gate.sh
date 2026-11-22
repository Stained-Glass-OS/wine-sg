#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ntdll/wow64: SystemLeapSecondInformation, 64- and 32-bit (patches/sg/8298; test/leapinfo-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/leapinfo-gate.sh
# Mutant: SG_MUTANT_NO_LEAP_SECOND_INFO (dlls/ntdll/unix/system.c, dlls/wow64/system.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-leapinfo.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/leapinfo-probe.c" 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
command -v i686-w64-mingw32-gcc >/dev/null && TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/probe32.exe" "$HERE/leapinfo-probe.c" 2>>"$T/cc.log"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
for p in probe probe32; do
    [ -f "$T/$p.exe" ] || continue
    echo "== $p"
    timeout -s KILL 60 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r' > "$T/$p.out"
    cat "$T/$p.out"
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
