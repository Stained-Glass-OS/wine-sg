#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A taskbar on every display (patches/sg/7751). David 2026-11-12: "many
# monitors, more than two even ... which gets the task bar, or if all of them
# get the task bar". As on Windows: the main taskbar on the main display, and
# with Settings' "Show taskbar on all displays" (Explorer\Advanced
# MMTaskbarEnabled, on by default) a bar on each other display, on the same
# edge, reserving that display's work area; "Show taskbar buttons on"
# (MMTaskbarMode) 2 leaves the main bar only the main display's windows.
#
# A headless sg-compositor with three outputs runs the shell's desktop over
# all of them (HostMonitors, 7749) and a probe reads the bars back.
#
#   WINE=/opt/wine-sg/bin/wine test/mmtaskbar-gate.sh
#   SG_COMPOSITOR_BUILD=<dir with sg-compositor>, or SG_COMPOSITOR_SRC=<checkout> to build one
# Mutants: SG_MUTANT_NO_SECOND_BARS, SG_MUTANT_MM_MAIN_SHOWS_ALL (explorer/systray.c),
# SG_MUTANT_SECOND_NO_PINS, SG_MUTANT_SECOND_NO_AUTOHIDE, SG_MUTANT_SECOND_NO_THUMBNAILS (7761),
# SG_MUTANT_SECOND_SYSTEM_MENU (7762, explorer/systray.c),
# SG_MUTANT_ICONS_AT_DESKTOP_ORIGIN (explorer/desktop.c), SG_MUTANT_DISPLAYCHANGE_DESKTOP_ONLY
# (win32u/sysparams.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0; CP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null && command -v xwininfo >/dev/null && command -v xdotool >/dev/null ||
    { echo "SKIP: needs mingw-w64, xwininfo and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
SRC="${SG_COMPOSITOR_SRC:-$HERE/../../sg-compositor}"
[ -d "$SRC" ] || SRC="${SG_REAL_HOME:-$HOME}/Stained-Glass-OS/sg-compositor"

T=$(mktemp -d /var/tmp/sg-mmtaskbar.XXXXXX)
# (the compositor waits for its child, the sleep, before it goes)
stop() { [ -n "$CP" ] && { pkill -P "$CP" 2>/dev/null; kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }; CP=""; }
cleanup() { "$WINESERVER" -k 2>/dev/null; stop; [ -n "${KEEP:-}" ] && echo "kept $T" || rm -rf "$T" "$SG_GATE_HOME"; }
trap cleanup EXIT INT TERM

if [ -n "${SG_COMPOSITOR_BUILD:-}" ] && [ -x "$SG_COMPOSITOR_BUILD/sg-compositor" ]; then
    COMP=$SG_COMPOSITOR_BUILD/sg-compositor
elif [ -d "$SRC" ]; then
    meson setup "$T/comp" "$SRC" -Dman-pages=disabled --buildtype=release >"$T/meson.log" 2>&1 &&
        ninja -C "$T/comp" >"$T/ninja.log" 2>&1 || { tail -20 "$T/meson.log" "$T/ninja.log"; echo "SKIP: cannot build the compositor"; exit 77; }
    COMP=$T/comp/sg-compositor
else
    echo "SKIP: no sg-compositor (SG_COMPOSITOR_BUILD or SG_COMPOSITOR_SRC)"; exit 77
fi
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/mmtaskbar-probe.exe" "$HERE/mmtaskbar-probe.c" -luser32 ||
    { fail "probe did not build"; exit 1; }

unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
mkdir -p "$T/prefix" "$T/comp-xdg" "$T/wine-xdg"
chmod 700 "$T/comp-xdg" "$T/wine-xdg"
export XDG_RUNTIME_DIR="$T/wine-xdg"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER

XDG_RUNTIME_DIR="$T/comp-xdg" WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=3 WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman \
    "$COMP" -L "$T/priv.sock" -C "$T/ctl.sock" -U "$(id -u)" -- \
    sh -c "echo \$DISPLAY > $T/disp; exec sleep 1200" >>"$T/comp.log" 2>&1 &
CP=$!
i=0; while [ ! -s "$T/disp" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
DISPLAY=$(cat "$T/disp" 2>/dev/null); export DISPLAY
[ -n "$DISPLAY" ] && [ "$DISPLAY" != ":0" ] || { echo "no display from the compositor"; cat "$T/comp.log"; exit 1; }
sleep 1

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
case "$(xwininfo -root 2>/dev/null | awk '/Width:/ {w=$2} /Height:/ {h=$2} END {print w "x" h}')" in
    3840x720) ;;
    *) echo "SKIP: the compositor did not make three 1280x720 outputs"; exit 77 ;;
esac
cp "$T/mmtaskbar-probe.exe" "$WINEPREFIX/drive_c/"
ADV='HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced'
R() { "$WINE" reg add "$1" /v "$2" /t REG_DWORD /d "$3" /f >/dev/null 2>&1; }
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d x11 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 3840x720 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v HostMonitors /d Y /f >/dev/null 2>&1
# a pinned program, as the image's default pins give every account
"$WINE" reg add 'HKLM\Software\Stained Glass\Taskbar' /v DefaultPins /t REG_MULTI_SZ /d 'Notepad|%SystemRoot%\system32\notepad.exe' /f >/dev/null 2>&1
"$WINESERVER" -w

P() { "$WINE" 'C:\mmtaskbar-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
state() { P state > "$T/$1.txt"; sed 's/^/      /' "$T/$1.txt" | sed "s/^      /      [$1] /"; }
works() { grep '^work' "$T/$1.txt" | sort -k2 -n | awk '{print $2","$3","$4","$5}' | tr '\n' ' '; }
seconds() { grep '^second' "$T/$1.txt" | sort -k2 -n | awk '{print $2","$3","$4","$5}' | tr '\n' ' '; }

SG_DESKTOP_DUMP='C:\desk.txt' timeout -s KILL 300 "$WINE" explorer /desktop=shell,3840x720 > "$T/explorer.out" 2>&1 &
# (explorer's desktop first: a probe started before it brought up the desktop
# itself, without the taskbar)
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q 'shell - Wine Desktop' && [ $i -lt 240 ]; do sleep 0.5; i=$((i + 1)); done
i=0; while ! P state | grep -q '^main' && [ $i -lt 120 ]; do sleep 1; i=$((i + 1)); done
sleep 3
"$WINE" 'C:\mmtaskbar-probe.exe' window mmwin1 200 100 >/dev/null 2>&1 &
"$WINE" 'C:\mmtaskbar-probe.exe' window mmwin3 2800 100 >/dev/null 2>&1 &
i=0; while [ "$(P state | grep -c '^mainbutton')" -lt 2 ] && [ $i -lt 60 ]; do sleep 1; i=$((i + 1)); done
sleep 1
state default
grep -q '^main 0 680 1280 720' "$T/default.txt" && pass "the main taskbar on the main display" ||
    fail "main bar: $(grep '^main' "$T/default.txt")"
[ "$(seconds default)" = "1280,680,2560,720 2560,680,3840,720 " ] &&
    pass "a bar at the bottom of each other display ($(seconds default))" || fail "other displays' bars: '$(seconds default)'"
[ "$(works default)" = "0,0,1280,680 1280,0,2560,680 2560,0,3840,680 " ] &&
    pass "each display's work area stops at its own bar" || fail "work areas: '$(works default)'"
# the pinned programs (the defaults) on the other displays' bars too, and the two windows
grep -q '^mainpins [1-9]' "$T/default.txt" && [ "$(grep '^second' "$T/default.txt" | awk '$7 > 0 && $9 == 2' | wc -l)" = 2 ] &&
    pass "the other displays' bars have the pinned programs and both windows ($(grep '^second' "$T/default.txt" | head -1 | cut -d' ' -f6-))" ||
    fail "other displays' bars' buttons: $(grep -E '^(second|mainpins)' "$T/default.txt" | tr '\n' ' ')"
[ "$(grep -c '^mainbutton' "$T/default.txt")" = 2 ] &&
    pass "Show taskbar buttons on all taskbars: the main bar has both windows" || fail "main bar buttons (all): $(grep '^mainbutton' "$T/default.txt" | tr '\n' ' ')"

R "$ADV" MMTaskbarMode 2; P apply; sleep 2
state where
grep -q '^mainbutton mmwin1' "$T/where.txt" && ! grep -q '^mainbutton mmwin3' "$T/where.txt" &&
    pass "taskbar where the window is open: the main bar has only the main display's window" ||
    fail "main bar buttons (where): $(grep '^mainbutton' "$T/where.txt" | tr '\n' ' ')"

grep '^second' "$T/where.txt" | awk '$7 == 0' | grep -q . && pass "taskbar where the window is open: no pinned programs on the other bars" ||
    fail "pins on the other bars (where): $(grep '^second' "$T/where.txt" | tr '\n' ' ')"

# thumbnails: the pointer resting on a window's button on display 2's bar
# (Start, the pinned program, then the windows) shows them above that bar
R "$ADV" MMTaskbarMode 0; P apply; sleep 2
xdotool mousemove 1395 700; sleep 0.3; xdotool mousemove 1400 701; sleep 2
state thumbs
t=$(grep '^thumbs' "$T/thumbs.txt")
echo "$t" | awk '{ exit !($2 >= 1280 && $4 <= 2560 && $5 <= 682) }' &&
    pass "thumbnails for a button on another display's bar, above it on that display ($t)" ||
    fail "thumbnails on display 2: '$t'"
xdotool mousemove 1900 300; sleep 1
# a right click on it: the window's jump list (the taskbar's menu), on that display
xdotool mousemove 1400 701; sleep 0.3; xdotool click 3; sleep 1.5
state menu
m=$(grep '^menu' "$T/menu.txt")
echo "$m" | awk '{ exit !($2 >= 1280 && $4 <= 2560 && $6 == "taskbar") }' &&
    pass "a right click on it: the window's jump list, on that display ($m)" || fail "right click on display 2: '$m'"
xdotool key Escape; sleep 0.5; xdotool mousemove 1900 300; sleep 1

# auto-hide: the other displays' bars tuck away too, and come back at the edge
R "$ADV" MMTaskbarMode 0; R 'HKCU\Software\Stained Glass\Taskbar' AutoHide 1; P apply
xdotool mousemove 1900 300; sleep 3
state hidden
[ "$(seconds hidden)" = "1280,718,2560,758 2560,718,3840,758 " ] && pass "auto-hide: the other displays' bars tuck away" ||
    fail "auto-hide: '$(seconds hidden)'"
xdotool mousemove 1900 719; sleep 2
state shown
[ "$(seconds shown)" = "1280,680,2560,720 2560,718,3840,758 " ] &&
    pass "and the one whose edge the pointer reaches comes back, the other stays away" || fail "auto-hide shown: '$(seconds shown)'"
xdotool mousemove 1900 300
R 'HKCU\Software\Stained Glass\Taskbar' AutoHide 0; P apply; sleep 2

R "$ADV" MMTaskbarMode 0; R "$ADV" MMTaskbarEnabled 0; P apply; sleep 2
state off
[ -z "$(seconds off)" ] && pass "Show taskbar on all displays off: only the main display has one" || fail "bars left: '$(seconds off)'"
[ "$(works off)" = "0,0,1280,680 1280,0,2560,720 2560,0,3840,720 " ] &&
    pass "and the other displays' work areas are whole again" || fail "work areas (off): '$(works off)'"

R "$ADV" MMTaskbarEnabled 1; R 'HKCU\Software\Stained Glass\Taskbar' Position 1; P apply; sleep 2
state top
[ "$(seconds top)" = "1280,0,2560,40 2560,0,3840,40 " ] && pass "the bars follow the main bar to the top edge" ||
    fail "bars at the top: '$(seconds top)'"

# --- the rightmost display made the main one while the shell runs (Settings'
# "Make this my main display": xrandr --primary, wine-sg 7752): the bars move
# to the others, now left of it, and the icons go with the main display ---
R 'HKCU\Software\Stained Glass\Taskbar' Position 3; P apply; sleep 2
OUT3=$(xrandr 2>/dev/null | awk '/ connected/ {print $1}' | sed -n 3p)
xrandr --output "$OUT3" --primary 2>/dev/null
sleep 4
state moved
[ "$(seconds moved)" = "-2560,680,-1280,720 -1280,680,0,720 " ] &&
    pass "the main display changed: the other displays' bars follow, left of it ($(seconds moved))" ||
    fail "bars after the main display changed: '$(seconds moved)'"
grep -q '^main 0 680 1280 720' "$T/moved.txt" && pass "the main taskbar on the new main display" ||
    fail "main bar after the change: $(grep '^main' "$T/moved.txt")"
tr -d '\r' < "$WINEPREFIX/drive_c/desk.txt" 2>/dev/null | grep '^item 0 ' > "$T/icon0.txt"
x=$(awk '{split($3, p, ","); print p[1]}' "$T/icon0.txt")
[ -n "$x" ] && [ "$x" -ge 2560 ] && [ "$x" -lt 2700 ] &&
    pass "the desktop icons are on the main display, from its corner ($(cat "$T/icon0.txt"))" ||
    fail "the first icon after the change: '$(cat "$T/icon0.txt")'"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
