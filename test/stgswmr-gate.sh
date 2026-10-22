#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ole32: structured storage: direct single-writer mode and IDirectWriterLock,
# MoveElementTo, SetElementTimes, stream LockRegion, STGM_NOSCRATCH (6006)
#   WINE=/opt/wine-sg/bin/wine test/stgswmr-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-stgswmr.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O2 -municode -o "$T/probe-$a.exe" "$HERE/stgswmr-probe.c" -lole32 -luuid 2>/dev/null ||
        { echo "FAIL  the $a probe did not build"; exit 1; }
done

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
for a in x86_64 i686; do
    echo "== $a"
    timeout -s KILL 60 "$WINE" "$T/probe-$a.exe" >"$T/out.raw" 2>/dev/null </dev/null
    tr -d '\r' <"$T/out.raw" >"$T/out"
    cat "$T/out"
    grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1
done
[ $rc = 0 ] || echo "RESULT: FAIL"
exit $rc
