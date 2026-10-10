#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Shared by the dplayx-*-gate.sh scripts (patches 2985..2989): builds a probe
# and the test service provider (test/dplayx-fakesp.c), registers the
# provider in a scratch prefix, runs the probe on a private Xvfb and checks
# its output and the log of the run.
#
#   dplayx_gate NAME PROBE_SOURCE 'fixme regex of the formerly stubbed functions'
#
# WINE=/opt/wine-sg/bin/wine, WINESERVER=... (a build tree: next to wine or in
# server/). SKIP (exit 77) when the tools are missing.
dplayx_gate() {
set -u
GNAME=$1; PROBE_SRC=$2; FIXME_RE=$3
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dplayx.XXXXXX)
# SG_DPLAYX_PREFIX=dir: a prefix that an earlier run left (the mutant runs
# use one: making a prefix takes most of the time)
KEEP_PREFIX=0
if [ -n "${SG_DPLAYX_PREFIX:-}" ]; then KEEP_PREFIX=1; PREFIX="$SG_DPLAYX_PREFIX"; else PREFIX="$T/prefix"; fi
export WINEPREFIX="$PREFIX" WINEDEBUG="-all,+err,fixme+dplay" WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/$PROBE_SRC" -ldxguid -lole32 -luuid -luser32 -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O1 -shared -o "$T/sgfakesp.dll" "$HERE/dplayx-fakesp.c" -ldxguid -luuid \
    || { echo "FAIL  test service provider did not build"; exit 1; }
if [ ! -d "$WINEPREFIX/drive_c" ]; then
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
reg() { timeout -s KILL 60 env DISPLAY= "$WINE" reg add "$@" /f >/dev/null 2>&1; }
SP='HKLM\Software\Microsoft\DirectPlay\Service Providers\SG Fake Service Provider'
LSP='HKLM\Software\Microsoft\DirectPlay\Lobby Providers\SG Fake Lobby Provider'
reg "$SP" /v Guid /d '{7d2ab7e1-93a3-4a0a-8a07-5b2c43b24e01}'
reg "$SP" /v Path /d 'C:\sgfakesp.dll'
reg "$SP" /v dwReserved1 /t REG_DWORD /d 0
reg "$SP" /v dwReserved2 /t REG_DWORD /d 0
reg "$LSP" /v Guid /d '{7d2ab7e2-93a3-4a0a-8a07-5b2c43b24e01}'
reg "$LSP" /v Path /d 'C:\sgfakesp.dll'
reg "$LSP" /v dwReserved1 /t REG_DWORD /d 0
reg "$LSP" /v dwReserved2 /t REG_DWORD /d 0
"$WINESERVER" -w
fi
cp "$T/probe.exe" "$T/sgfakesp.dll" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
# none of the formerly stubbed functions may log a FIXME
if grep -E "fixme:dplay:($FIXME_RE)" "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
grep -qx 'RESULT: FAIL' "$T/probe.out" || echo "FAIL  the probe did not finish (crash or timeout)"
exit 1
}
