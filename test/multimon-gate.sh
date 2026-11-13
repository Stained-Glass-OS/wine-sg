#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The shell's desktop over every monitor (patches/sg/7749). David 2026-11-12:
# with two monitors the desktop came up on one and the sign-in screen stayed on
# the other. The session's desktop (explorer /desktop=shell, HKCU\Software\Wine\
# Explorer HostMonitors=Y) now shows programs the host's monitors as they are,
# not one virtual monitor, and its window reaches all of them.
#
# A headless sg-compositor with two outputs (Xwayland reports them side by
# side, as a laptop and a monitor) runs the desktop and a probe in it:
#
#   - programs see two monitors, each where it is, the first primary
#   - the desktop's X window is both of them wide
#   - a window maximized on either monitor fills that monitor, not both
#   - without HostMonitors, one virtual monitor (Wine's own virtual desktop)
#
#   WINE=/opt/wine-sg/bin/wine test/multimon-gate.sh
#   SG_COMPOSITOR_BUILD=<dir with sg-compositor>, or SG_COMPOSITOR_SRC=<checkout> to build one
# Mutants: SG_MUTANT_ONE_VIRTUAL_MONITOR, SG_MUTANT_SPAN_CLIPPED (win32u/sysparams.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0; CP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null && command -v xwininfo >/dev/null || { echo "SKIP: needs mingw-w64 and xwininfo"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
SRC="${SG_COMPOSITOR_SRC:-$HERE/../../sg-compositor}"
[ -d "$SRC" ] || SRC="${SG_REAL_HOME:-$HOME}/Stained-Glass-OS/sg-compositor"

T=$(mktemp -d /var/tmp/sg-multimon.XXXXXX)
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
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/multimon-probe.exe" "$HERE/multimon-probe.c" -luser32 -lgdi32 ||
    { fail "probe did not build"; exit 1; }

unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
mkdir -p "$T/prefix" "$T/comp-xdg" "$T/wine-xdg"
chmod 700 "$T/comp-xdg" "$T/wine-xdg"
export XDG_RUNTIME_DIR="$T/wine-xdg"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER

# a headless compositor with two outputs (1280x720 each, side by side) whose child says its X display
start() {
    rm -f "$T/disp"
    XDG_RUNTIME_DIR="$T/comp-xdg" WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman \
        "$COMP" -L "$T/priv.sock" -C "$T/ctl.sock" -U "$(id -u)" -- \
        sh -c "echo \$DISPLAY > $T/disp; exec sleep 1200" >>"$T/comp.log" 2>&1 &
    CP=$!
    i=0; while [ ! -s "$T/disp" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
    DISPLAY=$(cat "$T/disp" 2>/dev/null); export DISPLAY
    [ -n "$DISPLAY" ] && [ "$DISPLAY" != ":0" ] || { echo "no display from the compositor"; cat "$T/comp.log"; exit 1; }
    sleep 1
}
# desktop OUT: the shell's desktop, the probe in it, until the probe is done
desktop() {
    rm -f "$T/$1"
    timeout -s KILL 180 "$WINE" explorer /desktop=shell,2560x720 'C:\multimon-probe.exe' "C:\\$1" >/dev/null 2>&1 &
    i=0; while ! grep -q '^ready' "$WINEPREFIX/drive_c/$1" 2>/dev/null && [ $i -lt 240 ]; do sleep 0.5; i=$((i + 1)); done
    tr -d '\r' < "$WINEPREFIX/drive_c/$1" > "$T/$1" 2>/dev/null
    xwininfo -root -children 2>/dev/null | grep 'Wine Desktop' > "$T/$1.x"
    "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
}
# band X1 Y1 X2 Y2 OUT: a rubber band dragged with the left button, the desktop's dump while it is held into OUT
band() {
    xdotool mousemove "$1" "$2" click 1; sleep 0.5   # the desktop active first
    xdotool mousemove "$1" "$2" mousedown 1; sleep 0.3
    xdotool mousemove $(( $1 + ($3 - $1) / 8 )) $(( $2 + ($4 - $2) / 8 )); sleep 0.3
    xdotool mousemove $(( $1 + ($3 - $1) / 2 )) $(( $2 + ($4 - $2) / 2 )); sleep 0.3
    xdotool mousemove "$3" "$4"; sleep 0.3; xdotool mousemove $(( $3 + 1 )) $(( $4 + 1 )); sleep 0.6
    tr -d '\r' < "$T/live.dump" > "$5" 2>/dev/null
    xdotool mouseup 1; sleep 1
}
start
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d x11 /f >/dev/null 2>&1
cp "$T/multimon-probe.exe" "$WINEPREFIX/drive_c/"
"$WINESERVER" -w
case "$(xwininfo -root 2>/dev/null | awk '/Width:/ {w=$2} /Height:/ {h=$2} END {print w "x" h}')" in
    2560x720) ;;
    *) echo "SKIP: the compositor did not make two 1280x720 outputs: $(xwininfo -root | grep -E 'Width|Height' | tr -d ' \n')"; exit 77 ;;
esac

# the two outputs, as the X server lists them (xrandr --listmonitors puts the
# primary one first, so it cannot tell which is which after a change)
OUT1=$(xrandr 2>/dev/null | awk '/ connected/ {print $1}' | sed -n 1p)
OUT2=$(xrandr 2>/dev/null | awk '/ connected/ {print $1}' | sed -n 2p)

# --- the shell's desktop over both monitors ---
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v HostMonitors /d Y /f >/dev/null 2>&1; "$WINESERVER" -w
desktop span.txt
grep -q '^monitors 2 virtual 0 0 2560 720' "$T/span.txt" && pass "programs see two monitors, the virtual screen both ($(head -1 "$T/span.txt"))" ||
    fail "not two monitors: $(head -3 "$T/span.txt" | tr '\n' ' ')"
grep -q '^monitor [^ ]* 0 0 1280 720 primary' "$T/span.txt" && grep -q '^monitor [^ ]* 1280 0 2560 720 -' "$T/span.txt" &&
    pass "each where it is, the first primary" || fail "monitors: $(grep '^monitor' "$T/span.txt" | tr '\n' ' ')"
grep -q '2560x720+0+0' "$T/span.txt.x" && pass "the desktop's X window is both monitors wide ($(sed 's/.*) *//' "$T/span.txt.x"))" ||
    fail "the desktop's X window: $(cat "$T/span.txt.x")"
grep -q '^max 0 [0-9]* [0-9]* 12[0-9][0-9] ' "$T/span.txt" && grep -q '^max 1 128[0-9] [0-9]* 25[0-9][0-9] ' "$T/span.txt" &&
    pass "a window maximized on either monitor fills that one ($(grep '^max' "$T/span.txt" | tr '\n' ' '))" ||
    fail "maximized: $(grep '^max' "$T/span.txt" | tr '\n' ' ')"

# --- the main display on the right (Settings: "Make this my main display",
# sg-settingsctl display primary: the X server's primary output) -- as on
# Windows the main display is at 0,0 and the one left of it at -1280 ---
if [ -n "$OUT2" ] && xrandr --output "$OUT2" --primary 2>/dev/null; then
    desktop right.txt
    grep -q '^monitors 2 virtual -1280 0 2560 720' "$T/right.txt" && grep -q '^monitor [^ ]* 0 0 1280 720 primary' "$T/right.txt" &&
        grep -q '^monitor [^ ]* -1280 0 0 720 -' "$T/right.txt" &&
        pass "the main display on the right: it is at 0,0, the other left of it ($(grep '^monitor' "$T/right.txt" | tr '\n' ' '))" ||
        fail "the main display on the right: $(grep -E '^monitor' "$T/right.txt" | tr '\n' ' ')"
    grep -q '2560x720+0+0' "$T/right.txt.x" && pass "the desktop's X window still covers both" ||
        fail "the desktop's X window with the main display on the right: $(cat "$T/right.txt.x")"
    xrandr --output "$OUT1" --primary 2>/dev/null
else
    fail "could not make the second output primary"
fi

# --- the main display changed while programs run (Settings' "Make this my
# main display" in a session: xrandr --primary, which sends only RandR's screen
# change, wine-sg 7752): a running program sees it, and what it shows at 0,0
# is on the new main display, the right one -- it stayed on the left ---
rm -f "$WINEPREFIX/drive_c/live.txt" "$WINEPREFIX/drive_c/live.txt.after" "$WINEPREFIX/drive_c/go"
rm -f "$T/live.dump"
SG_DESKTOP_DUMP="Z:$T/live.dump" timeout -s KILL 180 "$WINE" explorer /desktop=shell,2560x720 'C:\multimon-probe.exe' 'C:\live.txt' 'C:\go' >/dev/null 2>&1 &
i=0; while ! grep -q '^ready' "$WINEPREFIX/drive_c/live.txt" 2>/dev/null && [ $i -lt 240 ]; do sleep 0.5; i=$((i + 1)); done
if command -v xdotool >/dev/null; then
    sleep 3
    band 800 450 1100 640 "$T/live.band0"   # (clear of the probe's console, top left)
fi
xrandr --output "$OUT2" --primary 2>/dev/null
sleep 2
: > "$WINEPREFIX/drive_c/go"
i=0; while ! grep -q '^ready' "$WINEPREFIX/drive_c/live.txt.after" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.5; i=$((i + 1)); done
tr -d '\r' < "$WINEPREFIX/drive_c/live.txt.after" > "$T/live.after" 2>/dev/null
mm=$(xwininfo -root -tree 2>/dev/null | grep '"mmafter"' | head -1)
mb=$(xwininfo -root -tree 2>/dev/null | grep '"mmbefore"' | head -1)
grep -q '^monitor [^ ]* 0 0 1280 720 primary' "$T/live.after" && grep -q '^monitor [^ ]* -1280 0 0 720 -' "$T/live.after" &&
    pass "a running program sees the new main display ($(grep '^monitor' "$T/live.after" | tr '\n' ' '))" ||
    fail "a running program after the main display changed: $(grep '^monitor' "$T/live.after" | tr '\n' ' ')"
echo "$mm" | grep -q '+1290+10' && pass "and its window at 0,0 is on the main display, now the right one ($(echo "$mm" | sed 's/.*) *//'))" ||
    fail "its window at 10,10: '$mm'"
echo "$mb" | grep -q '+1290+10' && pass "and its window from before moved with the main display ($(echo "$mb" | sed 's/.*) *//'))" ||
    fail "its window from before the change: '$mb'"
# the desktop's window is over both displays again, from the new origin, and
# the mouse's points in it are where they are: a rubber band dragged out on the
# main display (now the right one) is drawn there -- it kept its old place, so
# the band was a display's width off, on the other display (David 2026-11-13)
if command -v xdotool >/dev/null; then
    band 2000 450 2300 640 "$T/live.band"
    grep -q 'deskrect=-1280,0,1280,720' "$T/live.band" && grep -q 'bandrect=2000,450,2302,642' "$T/live.band" &&
        pass "the desktop over both displays again, and a band dragged on the main display drawn there ($(grep -o 'bandrect=[^ ]*' "$T/live.band"), $(grep -o 'deskrect=[^ ]*' "$T/live.band"))" ||
        fail "the desktop after the main display changed: $(head -1 "$T/live.band" 2>/dev/null)"
    # and a click on an icon there selects it (it missed: the point was a display's width off)
    set -- $(tr -d '\r' < "$T/live.dump" | sed -n 's/^item 0 \([0-9-]*\),\([0-9-]*\) .*/\1 \2/p')
    if [ $# -eq 2 ]; then
        xdotool mousemove "$1" "$2"; sleep 0.5; xdotool click 1; sleep 0.5; xdotool click 1; sleep 1   # (the first may only activate the desktop)
        tr -d '\r' < "$T/live.dump" | head -1 | grep -q ' sel=0,' && pass "a click on an icon on the main display selects it (at $1,$2)" ||
            fail "a click on the first icon at $1,$2: $(tr -d '\r' < "$T/live.dump" | head -1)"
    else fail "no icon in the desktop's dump"; fi
fi
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
xrandr --output "$OUT1" --primary 2>/dev/null

# --- without HostMonitors: Wine's own virtual desktop, one monitor ---
"$WINE" reg delete 'HKCU\Software\Wine\Explorer' /v HostMonitors /f >/dev/null 2>&1; "$WINESERVER" -w
desktop one.txt
grep -q '^monitors 1 ' "$T/one.txt" && pass "without HostMonitors: one virtual monitor, as Wine has it ($(head -1 "$T/one.txt"))" ||
    fail "without HostMonitors: $(head -1 "$T/one.txt")"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
