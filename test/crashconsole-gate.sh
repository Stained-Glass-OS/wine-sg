#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A windowed program's crash shows the "Program Error" box and no console
# (patches/sg/5973). The debugger (winedbg, a console program) started for a
# program without a console got one of its own, and it came up as a window --
# an empty "Debugger" terminal reading "WineDbg attached to pid ..." behind
# the box (OneDrive's sync service, qa4 release 622). Windows' crash reporter
# shows no console.
#   WINE=/opt/wine-sg/bin/wine test/crashconsole-gate.sh
# Mutant: SG_MUTANT_DEBUGGER_CONSOLE (dlls/kernelbase/debug.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-crashconsole.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -mwindows -O1 -o "$T/probe.exe" "$HERE/crashconsole-probe.c" || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" 'C:\probe.exe' watch > "$T/watch" 2>/dev/null) &
W=$!
sleep 2
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 50 "$WINE" 'C:\probe.exe' crash >/dev/null 2>&1) &
wait $W
out=$(tr -d '\r' < "$T/watch")
printf '%s\n' "$out" | sed 's/^/      /'
case "$out" in *"dialog=1"*) pass "a windowed program's crash shows the Program Error box";; *) fail "no Program Error box";; esac
case "$out" in *"console=0"*) pass "and no console window (mutant DEBUGGER_CONSOLE)";; *) fail "a console window came up for the debugger";; esac
exit $RC
