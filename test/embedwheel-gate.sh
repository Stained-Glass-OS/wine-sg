#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The mouse wheel over a framed Linux program reaches it (David 2026-11-15:
# "Scrolling in Firefox doesn't work"). Firefox scrolls by XInput 2 (smooth
# scrolling); the frame's passive grab for the click that activates it
# (sg_embed.c) took the wheel's buttons too, and while that grab was active the
# wheel's XI2 events were the shell's. A stand-in program that scrolls by XI2,
# framed, is scrolled over: it sees every notch, and the wheel does not activate
# its frame (on Windows the wheel scrolls the window under the pointer without
# activating it). A click still activates it. Mutant SG_MUTANT_EMBED_GRAB_WHEEL.
#
#   WINE=/opt/wine-sg/bin/wine test/embedwheel-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset SESSION_MANAGER
for t in Xvfb xdotool xwininfo import cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-embedwheel.XXXXXX); XP=; AP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$AP" ] && kill "$AP" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/wheelapp" "$HERE/embedwheel-app.c" -lX11 -lXi || { echo "SKIP: embedwheel-app did not build (libxi-dev)"; exit 77; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
if [ -n "${EMBEDWHEEL_DISPLAY:-}" ]; then
    Xvfb ":$EMBEDWHEEL_DISPLAY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$EMBEDWHEEL_DISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
printf 'END\n' > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" SG_FAKE_FOCUS=1 XDG_RUNTIME_DIR="$T/run"
WINEDEBUG="${EMBED_DEBUG:--all}" "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 & EXPID=$!
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 4
cd "$WINEPREFIX/drive_c"

parent_of() { xwininfo -id "$1" -tree 2>/dev/null | awk '/Parent window id:/ {print $4; exit}'; }
root_id=$(xwininfo -root | awk '/Window id:/ {print $4; exit}')
framed() {
    _k=0
    while [ $_k -lt 80 ]; do
        _p=$(parent_of "$1"); [ -n "$_p" ] && [ "$_p" != "$root_id" ] && return 0
        sleep 0.25; _k=$((_k + 1))
    done
    return 1
}
logval() { awk -v k="$2" '$1 == k {print $2}' "$1" | tail -1; }
# the menu's colour where it is (embedmenu-app's 0xd02020)
menu_px() {
    _g=$(xwininfo -id "$1" 2>/dev/null)
    _x=$(echo "$_g" | awk '/Absolute upper-left X:/ {print $4}'); _y=$(echo "$_g" | awk '/Absolute upper-left Y:/ {print $4}')
    [ -n "$_x" ] || { echo none; return; }
    import -window root -crop "1x1+$((_x + 80))+$((_y + 60))" -depth 8 txt:- 2>/dev/null | tail -1 | grep -o '#[0-9A-F]\{6\}'
}
buttons() { "$WINE" probe.exe list SgLinuxWindow - 2>/dev/null | tr -d '\r' | grep -c .; }

L="$T/app.log"; : > "$L"
"$T/wheelapp" "$L" & AP=$!
i=0; while [ -z "$(logval "$L" main)" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
MAIN=$(logval "$L" main)
sleep 0.5
printf '%d shown - WheelTest\tWheel Test\nEND\n' "$MAIN" > "$T/list"
if framed "$MAIN"; then
    sleep 1.5
    g=$(xwininfo -id "$MAIN")
    mx=$(echo "$g" | awk '/Absolute upper-left X:/ {print $4}'); my=$(echo "$g" | awk '/Absolute upper-left Y:/ {print $4}')
    # another window active: Notepad
    "$WINE" notepad >/dev/null 2>&1 &
    sleep 4
    "$WINE" probe.exe move Notepad "" 640 460 360 220 >/dev/null 2>&1; sleep 1
    g=$(xwininfo -id "$MAIN")
    mx=$(echo "$g" | awk '/Absolute upper-left X:/ {print $4}'); my=$(echo "$g" | awk '/Absolute upper-left Y:/ {print $4}')
    "$WINE" probe.exe foreground SgLinuxWindow "Wheel Test" | grep -q 'foreground=0' || fail "test broken: Notepad is not active"
    : > "$T/commands"
    xdotool mousemove $((mx + 150)) $((my + 120)); sleep 0.3
    for k in 1 2 3 4 5 6; do xdotool click 5; sleep 0.15; done
    xdotool click 4; sleep 1
    n=$(grep -c '^wheel' "$L")
    [ "$n" -ge 7 ] && pass "every wheel notch over the framed program reaches it ($n XI2 scroll events for 7 notches)" \
        || fail "the framed program saw $n XI2 scroll events for 7 notches"
    "$WINE" probe.exe foreground SgLinuxWindow "Wheel Test" | grep -q 'foreground=0' \
        && pass "the wheel does not activate its frame" || fail "the wheel activated the frame"
    xdotool click 1; sleep 1.5
    "$WINE" probe.exe foreground SgLinuxWindow "Wheel Test" | grep -q 'foreground=1' \
        && pass "a click still activates it" || fail "a click did not activate it"
else
    fail "the program's window was not framed"
fi
kill "$AP" 2>/dev/null; AP=

echo
[ $RC = 0 ] && echo "embedwheel-gate: PASS" || echo "embedwheel-gate: FAIL"
exit $RC
