#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ntdll/server: the position of a synchronous file object with no unix fd (patches/sg/9700; test/pipepos-probe.c).
# As Windows' I/O manager keeps CurrentByteOffset of every synchronous file object whatever the device,
# FilePositionInformation queries and sets on a pipe succeed and leave its data alone (Python's tell()/lseek()
# on a subprocess pipe: calibre, Cura got 'unsupported info class').
# Mutant: SG_MUTANT_NO_SYNC_POSITION (server/fd.c).
#   WINE=/opt/wine-sg/bin/wine test/pipepos-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pipepos.XXXXXX)
P=pipepos-probe; LIBS=""
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/$P.exe" "$HERE/$P.c" $LIBS 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; echo "RESULT: FAIL"; exit 1; }
command -v i686-w64-mingw32-gcc >/dev/null &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/${P}32.exe" "$HERE/$P.c" $LIBS 2>>"$T/cc.log"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
for p in $P ${P}32; do
    [ -f "$T/$p.exe" ] || continue
    echo "== $p"
    (cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" </dev/null 2>/dev/null) | tr -d '\r' > "$T/$p.out"
    cat "$T/$p.out"
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
