#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# System profiles and streams with no buffer window can be written (patches/sg/9110; the version 8
# system profiles lacked the WMA 2 format data, so BeginWriting failed for all of them, and a stream
# never given a buffer window became a preroll of 4294967 s): each system profile writes and reads
# back; a no-window stream's file has a sane preroll and duration (test/wmprofwrite-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/wmprofwrite-gate.sh
# Needs gstreamer1.0-libav (avenc_wmav2) and gstreamer1.0-plugins-ugly (asfdemux, for the reader).
# Mutants: SG_MUTANT_WMVP_SYSTEM_NO_DATA (wmvcore profile.c), SG_MUTANT_WMVW_UNSET_WINDOW (wmvcore asfmux.c)
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
T=$(mktemp -d /var/tmp/sg-wmprofwrite.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmprofwrite-probe.c" -lole32 -luuid -lm || { fail "probe did not build"; exit 1; }
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
check system_profiles    "every system profile writes and reads back"
check no_window_written  "a stream with no buffer window writes"
check no_window_preroll  "with a sane preroll"
check no_window_duration "and a sane duration"
check done               "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
