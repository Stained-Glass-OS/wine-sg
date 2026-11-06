#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The default terminal (patches/sg/0787; David 2026-10-03: "All the terminal
# apps should support Tabs"): a console program that would open a console
# window of its own opens in the terminal instead -- wt.exe's App Paths entry,
# given the console's server handle -- while it stays the program it was: its
# exit code is its own (start /wait). A stand-in terminal (defterm-term.c)
# runs a headless conhost on the handed console, as sg-terminal does for a
# tab, types into it and keeps what it shows -- only a terminal that says it
# takes the handoff (SgConsoleHandoff = 1). With Default terminal set to the
# console host (DelegationTerminal), the console window opens as before.
#
#   WINE=/opt/wine-sg/bin/wine test/defterm-gate.sh   (mutant SG_MUTANT_NO_DEFTERM)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb xdotool "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-defterm.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -mwindows -o "$T/defterm-term.exe" "$HERE/defterm-term.c" || { fail "stand-in did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
D="$WINEPREFIX/drive_c"
cp "$T/defterm-term.exe" "$D/"
"$WINE" reg add 'HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\wt.exe' /ve /d 'C:\defterm-term.exe' /f >/dev/null 2>&1
cd "$D" || exit 1
# a terminal that does not say it takes the handoff is not given one (an older
# sg-terminal would leave the program on a console nobody serves)
"$WINE" start cmd /k "title SGNOHANDOFF" >/dev/null 2>&1 &
i=0; W=; while [ -z "$W" ] && [ $i -lt 40 ]; do sleep 0.5; W=$(xdotool search --name 'SGNOHANDOFF' 2>/dev/null | head -1); i=$((i + 1)); done
[ -n "$W" ] && [ ! -e "$D/defterm-args.txt" ] && pass "without SgConsoleHandoff = 1: the console window, as before" \
    || fail "without SgConsoleHandoff: window '$W', terminal $( [ -e "$D/defterm-args.txt" ] && echo started || echo 'not started')"
"$WINE" taskkill /f /im cmd.exe >/dev/null 2>&1; rm -f "$D/defterm-args.txt"
"$WINE" reg add 'HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\wt.exe' /v SgConsoleHandoff /t REG_DWORD /d 1 /f >/dev/null 2>&1
# a console program from a program with no console: a new console, in the terminal
timeout -s KILL 90 "$WINE" start /wait cmd /q /k "prompt $ " >/dev/null 2>&1; rc=$?
[ -s "$D/defterm-args.txt" ] && grep -q -- '--sg-handoff' "$D/defterm-args.txt" \
    && pass "the new console went to the terminal (--sg-handoff)" || fail "the terminal was not started"
grep -qi 'cmd' "$D/defterm-args.txt" 2>/dev/null && pass "with the program's title (cmd.exe's path)" || fail "no title"
grep -q '42' "$D/defterm-out.txt" 2>/dev/null && pass "the program runs in the terminal: typed 'set /a 40+2', it shows 42" \
    || fail "the terminal's console shows: '$(tr -d '\r' 2>/dev/null < "$D/defterm-out.txt" | head -c 300 | tr '\n' '|')'"
[ "$rc" = 7 ] && pass "it is still the program: start /wait gives its exit code (7)" || fail "start /wait gave $rc, not 7"
xdotool search --name 'cmd' >/dev/null 2>&1 && fail "a console window opened as well" || pass "no console window of its own"
# Default terminal: the console host
rm -f "$D/defterm-args.txt" "$D/defterm-out.txt"
"$WINE" reg add 'HKCU\Console\%%Startup' /v DelegationTerminal /d '{B23D10C0-E52E-411E-9D5B-C09FDF709C7D}' /f >/dev/null 2>&1
"$WINE" start cmd /k "title SGCONHOST" >/dev/null 2>&1 &
i=0; W=; while [ -z "$W" ] && [ $i -lt 40 ]; do sleep 0.5; W=$(xdotool search --name 'SGCONHOST' 2>/dev/null | head -1); i=$((i + 1)); done
[ -n "$W" ] && [ ! -e "$D/defterm-args.txt" ] && pass "Default terminal = console host: the console window, not the terminal" \
    || fail "with DelegationTerminal = conhost: window '$W', terminal $( [ -e "$D/defterm-args.txt" ] && echo started || echo 'not started')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
