#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ICaptureGraphBuilder2::RenderStream's source search (patches/sg/6516): VBI pins are rendered
# (it was E_NOTIMPL), the source's own unconnected outputs come before those further down, all
# connected is E_FAIL, a media type nothing has is S_OK (test/qcaprender-probe.c, filters of the
# probe's own).
#   WINE=/opt/wine-sg/bin/wine test/qcaprender-gate.sh
# Mutant: SG_MUTANT_QCAP_DEPTH_FIRST (qcap capturegraph.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-qcaprender.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/qcaprender-probe.c" -lstrmiids -lole32 -luuid || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 180 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check vbi             "a VBI-category pin is rendered to the sink"
check setup           "the probe's source and middle filter connect"
check own_first       "the source's own unconnected output is rendered first"
check then_downstream "then the middle filter's"
check all_connected   "everything connected is E_FAIL"
check no_such_type    "a media type nothing has: S_OK, nothing connected"
check done             "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
