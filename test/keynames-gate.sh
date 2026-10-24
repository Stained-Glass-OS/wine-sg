#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Key names (6093). GetKeyNameText named the keys that type no character by their X keysym: "Control_L",
# "Return", "Prior", "KP_Home", "BackSpace", "Super_L" -- Everything's File menu read "Control_L+N" where Windows
# shows "Ctrl+N", and shortcut settings everywhere did the same. They now have the names Windows gives them
# (win32u's key name tables): Ctrl, Right Ctrl, Enter, Page Up, Num 7, Backspace, Left Windows. Keys that type a
# character keep their keycap's character, from the layout.
#   WINE=/opt/wine-sg/bin/wine test/keynames-gate.sh   (mutant SG_MUTANT_KEYSYM_KEY_NAMES)
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
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-keynames.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY XAUTHORITY
XP=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -mwindows -o "$T/probe.exe" "$HERE/keynames-probe.c" 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
if [ -n "${SG_XDISPLAY:-}" ]; then
    Xvfb ":$SG_XDISPLAY" -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$SG_XDISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 800x600x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
timeout -s KILL 120 "$WINE" 'C:\probe.exe' 'C:\out.txt' >/dev/null 2>&1
out=$(tr -d '\r' < "$WINEPREFIX/drive_c/out.txt" 2>/dev/null)
printf '%s\n' "$out" | sed 's/^/      /'
expect() {
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$1: $2"
    else fail "$1: wanted '$2', got '$(printf '%s\n' "$out" | grep "^$1=" | cut -d= -f2- || echo none)'"; fi
}
expect ctrl Ctrl
expect rctrl "Right Ctrl"
expect shift Shift
expect rshift "Right Shift"
expect alt Alt
expect ralt "Right Alt"
expect ctrl_dontcare Ctrl
expect rctrl_dontcare Ctrl
expect a A
expect f1 F1
expect enter Enter
expect del Delete
expect space Space
expect esc Esc
expect bksp Backspace
expect pgup "Page Up"
expect num7 "Num 7"
expect caps "Caps Lock"
expect win "Left Windows"
expect done 1
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
