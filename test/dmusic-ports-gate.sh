#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dmusic's FIXME stubs (patches/sg/2971): test/dmusic-ports-probe.c drives the
# software synth port (channel groups, priorities, running stats, GetAppend,
# DeviceIoControl, no Thru), IDirectMusicBuffer (TotalTime, ResetReadPtr,
# GetNextEvent) and the master clock (SetMasterClock, external clock, Advise*).
# The run's log is also checked: the formerly stubbed functions may not log a FIXME.
# The MIDI ports need a MIDI device and are not exercised here.
#
#   WINE=/opt/wine-sg/bin/wine test/dmusic-ports-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dmusic, -DSG_MUTANT_x): PORT_GROUPS, PORT_PRIORITY, PORT_STATS, PORT_APPEND,
# BUF_NEXT, BUF_RESET, BUF_TOTAL, CLK_SET, CLK_EXT, CLK_ADVISE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dmusicports.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+dmusic WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/dmusic-ports-probe.c" -ldxguid -lole32 -luuid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
( cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 env DISPLAY= "$WINE" probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out" )
cat "$T/probe.out"
if grep -E 'fixme:dmusic:' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
