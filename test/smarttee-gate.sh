#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Smart Tee does not wait for itself (patches/sg/9109; it copied every sample into the source's
# own allocator under its lock, so a one-buffer source starved the preview and Stop hung: OpenCV's
# DirectShow capture froze in release()): frames through the preview and the capture pin from a
# one-buffer source, and the graph stops (test/smarttee-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/smarttee-gate.sh
# Mutants: SG_MUTANT_SMARTTEE_COPY (qcap smartteefilter.c)
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
T=$(mktemp -d /var/tmp/sg-smarttee.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/smarttee-probe.c" -lole32 -loleaut32 -luuid -lstrmiids || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 300 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check preview_connect    "a one-buffer source through the preview pin"
check preview_frames     "frames reach the preview sink"
check preview_data       "the preview frame is the picture"
check preview_stop       "the graph stops"
check capture_connect    "through the capture pin"
check capture_frames     "frames reach the capture sink"
check capture_data       "the capture frame is the picture"
check capture_stop       "the graph stops"
check done               "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
