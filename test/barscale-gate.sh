#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The taskbar grows with the screen as the title bars do (wine-sg 0814):
# sg-shell's looks write Style\Scale8, the screen's height in eighths of
# 800 px (1080p 11, 1200p 12), and size the title bars by it; the bar stayed
# 40 px whatever the screen, and looked small beside them. Under Xvfb, the
# shell's taskbar:
#   1. at scale 8 the bar is as before: 40 px
#   2. set to 12 while it runs (the looks' SPI_SETNONCLIENTMETRICS), the bar
#      is 60 px, and its buttons wider, without a restart
#   3. started at 12, it is 60 px from the start
#
#   WINE=/opt/wine-sg/bin/wine test/barscale-gate.sh   (mutant SG_MUTANT_BAR_NO_SCALE)
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
T=$(mktemp -d /var/tmp/sg-barscale.XXXXXX); XP=; EP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/barscale-probe.c" || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Stained Glass\Style' /v Scale8 /t REG_DWORD /d 8 /f >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run" SG_LOCK_CONTROL=/nonexistent
shell() { "$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 & sleep 8; }
bar() { "$WINE" "$T/probe.exe" bar 2>/dev/null | tr -d '\r'; }
val() { printf '%s\n' "$1" | sed -n "s/.*$2=\([0-9]*\).*/\1/p"; }

shell
b8=$(bar)
[ "$(val "$b8" height)" = 40 ] && pass "at scale 8 the bar is 40 px, as before" || fail "scale 8: $b8 (want height 40)"
"$WINE" "$T/probe.exe" set 12 >/dev/null 2>&1; sleep 2
b12=$(bar)
[ "$(val "$b12" height)" = 60 ] && pass "set to 12 while it runs, the bar is 60 px" || fail "set to 12: $b12 (want height 60)"
[ "$(val "$b12" widest)" -gt "$(val "$b8" widest)" ] 2>/dev/null \
    && pass "and its buttons are wider ($(val "$b8" widest) -> $(val "$b12" widest) px)" || fail "buttons: $b8 -> $b12"
"$WINESERVER" -k; sleep 1
shell
b=$(bar)
[ "$(val "$b" height)" = 60 ] && pass "started at 12, the bar is 60 px" || fail "started at 12: $b (want height 60)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
