#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A flyout after a Rounded -> Classic switch (patches/sg/9067, cal1's leftover). A window's
# surface goes when it is hidden; its X window kept the shape mask the old surface gave it
# (the Rounded style's round corners, 0492), and the new surface, with no shape of its own,
# never said the shape changed: the calendar, volume or network flyout, resized while hidden
# after the switch, came back showing only its old rounded area. A new surface starts
# unshaped now.
#
#   WINE=/opt/wine-sg/bin/wine test/roundswitch-gate.sh
# Mutant: winex11 built with -DSG_MUTANT_SURFACE_KEEPS_OLD_SHAPE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-roundswitch.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -mwindows -o "$T/rsprobe.exe" "$HERE/roundswitch-probe.c" -ldwmapi -luser32 -lgdi32 -ladvapi32 \
    || { fail "probe did not build"; exit 1; }
sg_gate_display || exit $?
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
sg_prefix_safe "$WINEPREFIX" || exit 1
cp "$T/rsprobe.exe" "$WINEPREFIX/drive_c/rsprobe.exe"
timeout -s KILL 120 "$WINE" explorer /desktop=shell,1024x768 'C:\rsprobe.exe' >/dev/null 2>&1
out=$(tr -d '\r' < "$WINEPREFIX/drive_c/out.txt" 2>/dev/null); echo "$out" | sed 's/^/      /'
field() { echo "$out" | grep "^$1" | tr ' ' '\n' | sed -n "s/^$2 *//p"; }
r=$(echo "$out" | grep '^rounded:')
[ "$r" = "rounded: corner 0 mid 1" ] && pass "in the Rounded style the flyout's corner is round (desktop shows through)" \
    || fail "the Rounded flyout: '$r'"
c=$(echo "$out" | grep '^classic:')
[ "$c" = "classic: corner 1 oldarea 1 newarea 1 newcorner 1" ] \
    && pass "after Classic, resized while hidden: all of its new size is drawn, corners square" \
    || fail "after the switch: '$c' (want the new area and the corners drawn)"
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
sg_kill_prefix "$WINEPREFIX"
[ $RC = 0 ] && echo "roundswitch-gate: all PASS" || echo "roundswitch-gate: FAIL"
exit $RC
