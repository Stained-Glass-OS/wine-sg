#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Xft settings are read once (patches/sg/0796). XGetDefault keeps
# nothing when the X server has no resources and there is no ~/.Xdefaults,
# as in our sessions: every font selected into a window's DC read the
# resource files again (hundreds of opens for one File Explorer folder on
# the QA VM, 2026-10-04). Here 300 font selections open ~/.Xdefaults a few
# times at most (Wine's own start, ~20; each selection read them, ~1400).
#
#   WINE=/opt/wine-sg/bin/wine test/xftdefault-gate.sh   (mutant SG_MUTANT_XFT_EVERY_FONT)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb strace "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xftdefault.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -mwindows -o "$T/probe.exe" "$HERE/xftdefault-probe.c" -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 800x600x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout 120 strace -f -e trace=openat -o "$T/st" "$WINE" "$T/probe.exe" >/dev/null 2>&1
n=$(grep -c '\.Xdefaults"' "$T/st")
[ "$n" -le 50 ] && pass "300 font selections: ~/.Xdefaults opened $n times" || fail "300 font selections opened ~/.Xdefaults $n times"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
