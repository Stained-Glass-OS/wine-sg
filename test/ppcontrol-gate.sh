#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Print job control while printing (6079, G-PSDRV): ControlPrintProcessor was a stub in winprint and wineps,
# and the spooler held its jobs lock for the whole print, so SetJob (and EnumJobs) from another thread waited
# until the job was done. Now fpScheduleJob releases the lock while the print processor prints, SetJob pause,
# resume and cancel reach the processor through ControlPrintProcessor, and the processor waits or stops
# between blocks (winprint) or records (wineps). test/ppcontrol-probe.c prints an 8 MB RAW job to a raw TCP
# printer on 127.0.0.1 that it is itself, reading slowly (in a network namespace of its own, unshare -rn):
# the job shows JOB_STATUS_PRINTING, nothing arrives while paused, all of it after resuming; a cancelled job
# stops short and leaves the queue. The host's CUPS is not used.
#   WINE=/opt/wine-sg/bin/wine test/ppcontrol-gate.sh   (mutants SG_MUTANT_PPCONTROL_LOCKED,
#   SG_MUTANT_PPCONTROL_NOPAUSE)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" unshare ip; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ppcontrol.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
export CUPS_SERVER="$T/no-cups.sock"
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/ppcontrol-probe.exe" "$HERE/ppcontrol-probe.c" -lwinspool -lws2_32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
cat > "$T/inner.sh" <<INNER
ip link set lo up
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/ppcontrol-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" ppcontrol-probe.exe 2>"\${PPC_LOG:-/dev/null}" </dev/null | tr -d '\\r' > "$T/probe.out"
"$WINESERVER" -k 2>/dev/null
INNER
unshare -rn sh "$T/inner.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
