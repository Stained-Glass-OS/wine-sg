#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Windows Media readers' sample times (patches/sg/6522): 10 fps and 30 fps WMVs written by
# the SDK's writer, read through WMCreateSyncReader and WMCreateReader, give their own frame
# count, times and durations (test/wmtiming-probe.c). Needs gstreamer1.0-libav (the WMV
# encoder and decoder) and gstreamer1.0-plugins-ugly (asfdemux).
#   WINE=/opt/wine-sg/bin/wine test/wmtiming-gate.sh
# Mutants: SG_MUTANT_WMVW_NO_FRAME_TIME, SG_MUTANT_WMVW_INDEX_NO_PREROLL (wmvcore asfmux.c): the 40 ms
# frames and the late start came from the writer (fixed in 6518), not the readers.
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
for so in libgstlibav.so libgstasf.so; do
    ls /usr/lib/*/gstreamer-1.0/$so >/dev/null 2>&1 || { echo "SKIP: no $so (gstreamer1.0-libav, -plugins-ugly)"; exit 77; }
done
T=$(mktemp -d /var/tmp/sg-wmtiming.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmtiming-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
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
check write10  "the writer writes a 10 fps WMV"
check sync10   "the synchronous reader gives its 20 frames at 100 ms"
check async10  "the asynchronous reader too"
check write30  "the writer writes a 30 fps WMV"
check sync30   "the synchronous reader gives its 60 frames at 33.3 ms"
check async30  "the asynchronous reader too"
check writeav10 "a 10 fps WMV with a WMA stream"
check syncav10  "the synchronous reader gives its 20 frames at 100 ms"
check asyncav10 "the asynchronous reader too"
check writeav30 "a 30 fps WMV with a WMA stream"
check syncav30  "the synchronous reader gives its 60 frames at 33.3 ms"
check asyncav30 "the asynchronous reader too"
check done             "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
