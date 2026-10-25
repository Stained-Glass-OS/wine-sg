#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Glyph outlines of PostScript-flavoured OpenType fonts (patches/sg/6537): a CFF font's curves are
# cubic. GetGlyphOutline read them as TrueType's quadratic B-splines, so GGO_NATIVE and GGO_BEZIER
# gave broken shapes -- through cairo (RawTherapee's text, GIMP) and GDI+ path text the Inter that
# stands in for Segoe UI lost strokes (N without its diagonal). Now GGO_NATIVE gives them as
# TT_PRIM_CSPLINE, as Windows does, GGO_BEZIER passes them on, and the glyph drawn from either
# outline covers what TextOut draws; a TrueType font (Tahoma) is as before (test/cffoutline-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/cffoutline-gate.sh
# Needs the Inter font (fonts-inter, the Segoe UI stand-in). Mutant: SG_MUTANT_CFF_AS_QSPLINE
# (dlls/win32u/freetype.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" fc-list; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
fc-list :family=Inter file | grep -q '\.otf' || { echo "SKIP: no Inter OpenType (CFF) font"; exit 77; }
T=$(mktemp -d /var/tmp/sg-cffoutline.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/cffoutline-probe.c" -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
inter=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' Inter 2>/dev/null | tr -d '\r')
tahoma=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' Tahoma 2>/dev/null | tr -d '\r')
printf '%s\n' "$inter" | sed 's/^/      Inter:  /'
printf '%s\n' "$tahoma" | sed 's/^/      Tahoma: /'
yes_no() { if [ "$1" = 1 ]; then pass "$2"; else fail "$2${3:+ ($3)}"; fi; }
yes_no "$(printf '%s\n' "$inter" | grep -q '^face=Inter$' && echo 1)" "the Inter OpenType (CFF) font is selected"
yes_no "$(printf '%s\n' "$inter" | grep -Eq '^native csplines=[1-9][0-9]* qsplines=0$' && echo 1)" \
    "GGO_NATIVE gives its cubic curves as TT_PRIM_CSPLINE, none as QSPLINE" "$(printf '%s\n' "$inter" | grep '^native')"
yes_no "$(printf '%s\n' "$inter" | grep -q '^native_matches=1$' && echo 1)" "the GGO_NATIVE outline covers what TextOut draws (N with its diagonal)"
yes_no "$(printf '%s\n' "$inter" | grep -q '^bezier_matches=1$' && echo 1)" "and so does the GGO_BEZIER outline"
yes_no "$(printf '%s\n' "$tahoma" | grep -q '^native_matches=1$' && printf '%s\n' "$tahoma" | grep -q '^bezier_matches=1$' && echo 1)" \
    "a TrueType font (Tahoma): both outlines as before"
yes_no "$(printf '%s\n%s\n' "$inter" "$tahoma" | grep -c '^done=1$' | grep -qx 2 && echo 1)" "the probe ran to the end, twice"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
