#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# GDI+ GdipWindingModeOutline and GdipSetImageAttributesOutputChannel (patches/sg/6505): both were
# NotImplemented. The outline is what the path fills under the winding rule (overlapping figures
# merged, a pentagram's middle kept, a hole turned the other way kept); an output channel draws the
# image as one CMYK channel in gray. test/gdipoutline-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/gdipoutline-gate.sh
# Mutants: SG_MUTANT_GDIP_OUTLINE_KEEP_ALL (graphicspath.c), SG_MUTANT_GDIP_OUTPUT_CHANNEL_NONE (graphics.c).
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
T=$(mktemp -d /var/tmp/sg-gdipoutline.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/gdipoutline-probe.c" -lm || { fail "probe did not build"; exit 1; }
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
check union       "two overlapping squares: one figure of 8 corners"
check union_area  "which fills their union"
check star        "a pentagram: its 10-pointed rim"
check star_middle "with the middle filled"
check hole        "a hole turned the other way: two figures"
check hole_area   "the hole stays empty"
check channel_c   "output channel C of red: white"
check channel_m   "output channel M of red: black"
check channel_y   "output channel Y of red: black"
check channel_k   "output channel K of red: white"
check channel_bad "an unknown channel is InvalidParameter"
check done        "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
