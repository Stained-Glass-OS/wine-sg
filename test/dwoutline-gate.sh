#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DirectWrite glyph outlines unhinted, at any em size (patches/sg/6538): GetGlyphRunOutline loaded
# the glyph at the requested em size, hinted, and rounded its points to 1/64 of a DIP. cairo asks for
# outlines at an em size of 1 and scales them up, so Pango's path text (RawTherapee's image info,
# GTK programs drawing text as paths) came out as broken glyphs. Now the design outline is scaled to
# the em size, as on Windows: 'N' (Tahoma Bold) at em size 1, scaled by 100, matches the outline at
# em size 100, and its points are not on a 1/64 grid (test/dwoutline-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/dwoutline-gate.sh
# Mutant: SG_MUTANT_DWRITE_OUTLINE_HINTED (dlls/dwrite/freetype.c).
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
T=$(mktemp -d /var/tmp/sg-dwoutline.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -Wno-incompatible-pointer-types -o "$T/probe.exe" "$HERE/dwoutline-probe.c" -ldwrite -luuid || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check family      "Tahoma is in the system font collection"
check same_points "the outline at em size 1 has the same points as at em size 100"
check scales      "scaled by 100 it matches the outline at em size 100 (within 0.5 DIP)"
check unhinted    "the points are the design outline's, not on a 1/64 grid"
check done        "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
