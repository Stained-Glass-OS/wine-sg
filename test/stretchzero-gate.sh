#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# StretchBlt with an empty source or destination (patches/sg/0789): nothing is
# drawn and the program goes on. From the screen's DC -- which has no device
# rectangle, so an empty source passed the clipping -- the scaling divided by
# the source's width in win32u's Unix side, and the signal there ended the
# program (MeediOS on David's Latitude and its developer's machine, exit code
# 0x94, 2026-10-03). A normal stretch still draws.
#
#   WINE=/opt/wine-sg/bin/wine test/stretchzero-gate.sh   (mutant SG_MUTANT_STRETCH_DIVIDES_BY_ZERO)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-stretchzero.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/stretchzero-probe.c" -lgdi32 -luser32 -lmsimg32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 800x600x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 120 "$WINE" "$T/probe.exe" > "$T/out" 2>/dev/null; rc=$?
tr -d '\r' < "$T/out" > "$T/o"
[ "$rc" = 0 ] && grep -q '^DONE' "$T/o" && pass "the program goes on after every empty stretch (exit $rc)" \
    || fail "the program ended (exit $rc) after: $(tail -1 "$T/o")"
grep -q '^mem<-screen 10x10<-0x10 ' "$T/o" && pass "from the screen with a source of no width: returned ($(grep '^mem<-screen 10x10<-0x10' "$T/o"))" \
    || fail "from the screen with a source of no width: no answer"
grep -q '^alpha ' "$T/o" && pass "AlphaBlend from the screen with a source of no width returns" || fail "AlphaBlend did not return"
grep -q '^stretched 0000ff$' "$T/o" && pass "a normal stretch still draws (red where it was stretched)" || fail "normal stretch: $(grep '^stretched' "$T/o")"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
