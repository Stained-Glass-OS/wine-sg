#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# EMF+ records GDI+ on Windows writes (Office's documents, charts and clip art), played back
# (patches/sg/6504): DrawLines, FillPolygon, DrawBeziers, DrawCurve, DrawClosedCurve, DrawString with
# a StringFormat object, OffsetClip, ResetClip, SetTextContrast, Comment -- each was "not
# implemented" and left out of the picture. The probe (test/gdipemfplus-probe.c) writes the records
# by hand, as [MS-EMFPLUS] lays them out, and plays them onto a bitmap.
#   WINE=/opt/wine-sg/bin/wine test/gdipemfplus-gate.sh
# Mutant: SG_MUTANT_GDIP_EMFPLUS_UNPLAYED (metafile.c).
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
T=$(mktemp -d /var/tmp/sg-gdipemfplus.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/gdipemfplus-probe.c" -lgdi32 || { fail "probe did not build"; exit 1; }
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
check enumerated               "the metafile is enumerated"
for r in comment drawlines fillpolygon drawbeziers drawcurve drawclosedcurve offsetclip resetclip settextcontrast drawstring object; do
    check "played_$r" "EMF+ record $r is played"
done
check pixel_lines              "DrawLines (compressed points) draws its pen"
check pixel_polygon            "FillPolygon (relative points, a colour) fills"
check pixel_beziers            "DrawBeziers draws"
check pixel_curve              "DrawCurve draws"
check pixel_clipped_out        "SetClipRect + OffsetClip: nothing outside the moved clip"
check pixel_clipped_in         "and the fill inside it"
check pixel_string             "DrawString (font and string format objects) writes its text"
check done                     "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
