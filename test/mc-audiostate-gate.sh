#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windows.media.mediacontrol's eight audiostatemonitorapi exports (patches/sg/2982-2984):
# test/mc-audiostate-probe.exe calls every Create{Capture,Render}AudioStateMonitor*
# function and checks the monitor it returns (interfaces, reference counts, idle
# sound level Full, callback handles, argument checks). The gate also fails when the
# DLL logs a FIXME on the mediacontrol channel (these were `@ stub` exports).
#
#   WINE=/opt/wine-sg/bin/wine test/mc-audiostate-gate.sh
# Mutants (windows.media.mediacontrol, -DSG_MUTANT_x): ASM_LEVEL (sound level Low),
# ASM_HANDLE (the same handle every time), ASM_REF (AddRef does not count),
# ASM_DEVID (a NULL device id is accepted).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mcaudio.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+mediacontrol WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/mc-audiostate-probe.c" \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= WINEDEBUG=-all "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
timeout -s KILL 120 env DISPLAY= "$WINE" probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
cat "$T/probe.out"
if grep -E 'fixme:mediacontrol' "$T/stderr.log"; then
    echo "FAIL  the DLL logged a FIXME"
    exit 1
fi
echo "PASS  no FIXME logged"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
