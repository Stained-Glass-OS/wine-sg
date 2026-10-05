#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The taskbar's drawn art is smooth (wine-sg 0856): GDI draws lines,
# ellipses and polygons without antialiasing, so the search magnifier, the
# Start mark's four panes and the Glass look's orb had stepped edges, worse
# on a scaled bar (David 2026-10-05: "low quality"). They are drawn four
# times larger and averaged down. Under Xvfb, the shell's taskbar at 100%,
# its edges counted in distinct colours (jagged: only the art's own colours
# and the bar's; smooth: the blends between them):
#   1. the Start mark (flat look): at least 12 colours (jagged: 5)
#   2. the search button's magnifier: at least 8 (jagged: 2)
#   3. the Glass orb's edge, where it is diagonal: at least 24 (jagged: 13)
#
#   WINE=/opt/wine-sg/bin/wine test/smoothicons-gate.sh
#   (mutants SG_MUTANT_JAGGED_MARK, SG_MUTANT_JAGGED_MAGNIFIER, SG_MUTANT_JAGGED_ORB)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb import convert; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-smoothicons.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Search' /v SearchboxTaskbarMode /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run" SG_LOCK_CONTROL=/nonexistent
shell() { "$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 & sleep 8; import -window root "$T/$1.png"; }
colours() { convert "$T/$1.png" -crop "$2" +repage -format %k info:; }

shell flat
n=$(colours flat 36x36+6+730)
[ "$n" -ge 12 ] 2>/dev/null && pass "the Start mark is smooth ($n colours)" || fail "the Start mark is jagged: ${n:-?} colours (want 12 or more)"
n=$(colours flat 24x24+66+736)
[ "$n" -ge 8 ] 2>/dev/null && pass "the magnifier is smooth ($n colours)" || fail "the magnifier is jagged: ${n:-?} colours (want 8 or more)"
"$WINESERVER" -k; sleep 1
"$WINE" reg add 'HKCU\Software\Stained Glass\Taskbar' /v Style /t REG_DWORD /d 2 /f >/dev/null 2>&1
"$WINESERVER" -w
shell glass
n=$(colours glass 8x8+11+731)
[ "$n" -ge 24 ] 2>/dev/null && pass "the Glass orb's edge is smooth ($n colours)" || fail "the Glass orb's edge is jagged: ${n:-?} colours (want 24 or more)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
