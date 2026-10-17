#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Alt+F4 (and every Alt or Ctrl combination) with a low-level keyboard hook
# that is a moment slow -- explorer always has one (the Win-key shortcuts).
# The server judged a key against the desktop's key state, which a key enters
# only after the hook has seen it: F4 right after Alt came without Alt, a plain
# F4 and then Alt's menu, and the window did not close (patch 5935; David,
# 2026-10-17: the keyboard "not mapped" to the window clicked; the Alarms &
# Clock gate's Alt+F4). Here: a hook taking 40 ms a key in one program, a
# window in another, typing then Alt+F4, three times: the letters in order and
# the window closed each time. The hook chains to explorer's: called from the
# hook's thread, the next hook in the chain took the next key's hook inside it
# and keys came out last first (win32u, the same patch).
#   WINE=/opt/wine-sg/bin/wine test/althook-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_MODS_AFTER_HOOK (server), SG_MUTANT_LL_CHAIN_NESTS (win32u).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in Xvfb xdotool $MINGW; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
T=$(mktemp -d /var/tmp/sg-althook.XXXXXX)
export HOME="$T/home" WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
mkdir -p "$HOME"
XP=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/althook-probe.c" -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x700 >/dev/null 2>&1 &
sleep 5
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
"$WINE" probe.exe hook >/dev/null 2>&1 &
sleep 2
rc=0
for n in 1 2 3; do
    "$WINE" probe.exe window > "$T/w$n.out" 2>/dev/null &
    P=$!
    sleep 2.5
    xdotool mousemove 300 250 click 1; sleep 0.5
    xdotool type --delay 15 qwerty; xdotool key alt+F4
    wait $P
    r=$(tr -d '\r' < "$T/w$n.out" | tr '\n' ' ')
    [ "$r" = "TYPED qwerty CLOSED " ] && echo "PASS  with slow keyboard hooks, 'qwerty' arrives in order and Alt+F4 closes the window ($n)" \
        || { echo "FAIL  with slow keyboard hooks ($n): $r (want: TYPED qwerty CLOSED)"; rc=1; }
done
exit $rc
