#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ntdll: RtlGetCurrentProcessorNumberEx answers group 0 and the current processor with no FIXME line per call
# (patches/sg/8262; test/procnumex-probe.c): the probe calls it 1000 times; stderr (WINEDEBUG fixme) has no semi-stub line.
#   WINE=/opt/wine-sg/bin/wine test/procnumex-gate.sh
# Mutant: SG_MUTANT_PROCNUMEX_FIXME (dlls/ntdll/rtl.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-procnumex.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/procnumex-probe.c" 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
WINEDEBUG=fixme+all timeout -s KILL 60 "$WINE" "$T/probe.exe" >"$T/out.raw" 2>"$T/err" </dev/null
n=$(grep -c "RtlGetCurrentProcessorNumberEx" "$T/err")
[ "$n" = 0 ] && echo "PASS  no FIXME line for it on stderr" >>"$T/out.raw" || echo "FAIL  $n FIXME lines for it on stderr" >>"$T/out.raw"
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
