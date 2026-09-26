#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Magnifier and the On-Screen Keyboard reach sg-shell's programs
# (patches/sg/0181, 0182).
#
# magnify.exe and osk.exe are system32 programs on Windows; wine-sg ships them
# as launchers for what App Paths registers (0181), from 64- and 32-bit
# callers with their arguments. Explorer's keys (0182): Win+Plus starts
# Magnifier, or -- when its window (class SgMagnifier) is open -- tells it to
# zoom in (WM_COMMAND 0x101); Win+Minus zooms out (0x102); Win+Esc closes it
# (WM_CLOSE); none of them starts anything else. Win+Ctrl+O starts the
# On-Screen Keyboard, or closes it when it is open (OSKMainClass). And an
# appbar's space comes off the work area (the docked Magnifier and
# keyboard), given back when it is removed or its window is gone.
#
# The programs themselves are stand-ins (test/a11y-probe.c) logging their
# command lines and the messages they get; sg-shell's own gates
# (magnify-check.sh, osk-check.sh) drive the real ones.
#
#   WINE=/opt/wine-sg/bin/wine test/a11y-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
DPY="${DPY:-175}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null && command -v "$MINGW32" >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-a11y.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER DISPLAY=":$DPY"
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -f "/tmp/.X$DPY-lock"; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/a11y-probe.c" -lshell32 -luser32 &&
"$MINGW32" -municode -O2 -o "$T/probe32.exe" "$HERE/a11y-probe.c" -lshell32 -luser32 || { fail "probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe64.exe" "$C/probe64.exe"; cp "$T/probe32.exe" "$C/probe32.exe"
cp "$T/probe64.exe" "$C/mag-standin.exe"; cp "$T/probe64.exe" "$C/osk-standin.exe"
for n in magnify osk; do
    for d in system32 syswow64; do
        [ -f "$C/windows/$d/$n.exe" ] && pass "$d\\$n.exe exists" || fail "no $d\\$n.exe"
    done
done
AP='HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths'
"$WINE" reg add "$AP\\magnify.exe" /ve /d 'C:\mag-standin.exe' /f >/dev/null 2>&1
"$WINE" reg add "$AP\\osk.exe" /ve /d 'C:\osk-standin.exe' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x768 /f >/dev/null 2>&1
"$WINESERVER" -w

LOG="$C/standin.log"; : > "$LOG"
P() { (cd "$C" && "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
count() { grep -c "$1" "$LOG" 2>/dev/null; }
# wait until LOG has at least N lines matching RE
wait_count() { i=0; while [ "$(count "$1")" -lt "$2" ] && [ $i -lt 20 ]; do sleep 0.5; i=$((i + 1)); done; [ "$(count "$1")" -ge "$2" ]; }

WINEDEBUG=err+all,trace+explorer "$WINE" explorer /desktop=shell,1024x768 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 2

# --- the launchers --------------------------------------------------------------------------------
P probe64.exe run 'magnify.exe /lens /zoom:300'
wait_count '^cmdline=' 1 && tail -1 "$LOG" | grep -q 'mag-standin.exe" /lens /zoom:300$' \
    && pass "64-bit CreateProcess(magnify.exe /lens /zoom:300) reaches App Paths' Magnifier with its arguments" \
    || fail "magnify.exe from 64-bit: $(tail -1 "$LOG")"
"$WINE" cmd /c 'taskkill /im mag-standin.exe /f' >/dev/null 2>&1; sleep 1
P probe32.exe run 'osk.exe'
wait_count '^cmdline=' 2 && tail -1 "$LOG" | grep -q 'osk-standin.exe"$' \
    && pass "32-bit CreateProcess(osk.exe) reaches App Paths' On-Screen Keyboard" || fail "osk.exe from 32-bit: $(tail -1 "$LOG")"
"$WINE" cmd /c 'taskkill /im osk-standin.exe /f' >/dev/null 2>&1; sleep 1

# --- the keys -------------------------------------------------------------------------------------------
# keys reach the desktop's X window once a window is on it: a Notepad, clicked
"$WINE" notepad >/dev/null 2>&1 &
sleep 3; xdotool mousemove 200 150 click 1; sleep 1
: > "$LOG"
xdotool key super+minus; xdotool key super+Escape; sleep 2
[ "$(count '^cmdline=')" = 0 ] && pass "Win+Minus and Win+Esc start nothing when Magnifier is not open" \
    || fail "they started: $(cat "$LOG")"
xdotool key super+equal
wait_count '^cmdline=' 1 && grep -q 'mag-standin.exe"$' "$LOG" && pass "Win+Plus starts Magnifier (magnify.exe)" \
    || fail "Win+Plus: $(cat "$LOG")"
sleep 1
xdotool key super+equal
wait_count '^command 257$' 1 && [ "$(count '^cmdline=')" = 1 ] && pass "Win+Plus again tells the open Magnifier to zoom in (WM_COMMAND 0x101)" \
    || fail "second Win+Plus: $(tr '\n' '|' < "$LOG")"
xdotool key super+KP_Add
wait_count '^command 257$' 2 && pass "Win+keypad Plus zooms in too" || fail "Win+KP_Add: $(tr '\n' '|' < "$LOG")"
xdotool key super+minus
wait_count '^command 258$' 1 && pass "Win+Minus zooms out (WM_COMMAND 0x102)" || fail "Win+Minus: $(tr '\n' '|' < "$LOG")"
xdotool key super+Escape
wait_count '^close$' 1 && pass "Win+Esc closes Magnifier (WM_CLOSE)" || fail "Win+Esc: $(tr '\n' '|' < "$LOG")"
: > "$LOG"
xdotool key super+ctrl+o
wait_count '^cmdline=' 1 && grep -q 'osk-standin.exe"$' "$LOG" && pass "Win+Ctrl+O starts the On-Screen Keyboard (osk.exe)" \
    || fail "Win+Ctrl+O: $(cat "$LOG")"
sleep 1
xdotool key super+ctrl+o
wait_count '^close$' 1 && [ "$(count '^cmdline=')" = 1 ] && pass "Win+Ctrl+O again closes it" || fail "second Win+Ctrl+O: $(tr '\n' '|' < "$LOG")"

# --- appbars and the work area ----------------------------------------------------------------------------
W0=$(P probe64.exe workarea)
[ "$W0" = "0 0 1024 728" ] && pass "the work area is the screen less the taskbar ($W0)" || fail "work area at start: $W0"
set -- $(P probe64.exe appbar top 150 | tr '\n' ' ') - - - - - - - -
[ "$1 $2 $3 $4" = "0 150 1024 728" ] && pass "a 150-pixel top appbar takes the top of the work area" || fail "with a top appbar: $1 $2 $3 $4"
[ "$5 $6 $7 $8" = "0 0 1024 728" ] && pass "removing it gives the space back" || fail "after ABM_REMOVE: $5 $6 $7 $8"
set -- $(P probe32.exe appbar-leak) - - - -
[ "$1 $2 $3 $4" = "80 0 1024 728" ] && pass "a 32-bit program's left appbar takes the left of the work area" || fail "left appbar: $1 $2 $3 $4"
set -- $(P probe64.exe appbar top 100 | tr '\n' ' ') - - - - - - - -
[ "$1 $2 $3 $4" = "0 100 1024 728" ] && [ "$5 $6 $7 $8" = "0 0 1024 728" ] \
    && pass "an appbar whose program has gone gives its space back" || fail "after the leaked appbar: $1 $2 $3 $4 / $5 $6 $7 $8"

[ $RC = 0 ] && echo "a11y-gate: PASS" || echo "a11y-gate: FAIL"
exit $RC
