#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# EMF+ pens with optional data (patches/sg/8008): played back, a pen's dash pattern, transform,
# dash cap, alignment and compound lines were skipped as "not supported" (a dashed pen drew solid);
# recorded, a pen's alignment was written as a float and compound lines not at all. The probe
# (test/gdippen-probe.c) writes a pen record by hand and plays it, and records an inset compound pen.
#   WINE=/opt/wine-sg/bin/wine test/gdippen-gate.sh
# Mutant: SG_MUTANT_EMFP_PENDATA (metafile.c, playback).
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
T=$(mktemp -d /var/tmp/sg-gdippen.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/gdippen-probe.c" -lgdi32 || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { fail "probe could not start"; exit 1; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check pen_object_played        "a pen object with every kind of optional data is read"
check solid_line               "a solid pen draws the whole line"
check dashed_line              "a pen's dash pattern (PenDataDashedLine) dashes the line"
check recorded_pen             "a recorded pen object is written"
check recorded_align_int       "its alignment is written as an integer"
check recorded_compound        "its compound lines are written"
check done                     "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
