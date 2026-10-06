#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A pen and a touch screen reach Windows programs (patches/sg/1000, 1001):
# sg-compositor's test build (-Dtest-tablet=true) feeds a pen and a touch
# screen from a FIFO through its real input path; Xwayland makes them X
# input devices; test/penpointer-probe.c is a drawing program.
#
#  1000: the pen's pointer messages -- WM_POINTERDOWN/UPDATE/UP with
#        GetPointerPenInfo's pressure (0..1024) and tilt, GetPointerDevices,
#        GetPointerDeviceRects, the barrel button, the mouse messages it makes
#        marked as a pen's (MI_WP_SIGNATURE); Wintab's packets alongside; and
#        a full-screen program's (whose cursor clipping grabs the pointer).
#        Before: no pointer message, GetPointerPenInfo failed -- no pressure.
#  1001: two fingers scroll (the mouse wheel) and pinch (Ctrl+wheel); one
#        finger still clicks. Before: a second finger did nothing. (Since
#        1150 for a program leaving touches to DefWindowProc: the probe does
#        so here, PENPROBE_TOUCH_DEFPROC; test/touch-gate.sh tests the rest.)
#
#   WINE=<build>/wine WINESERVER=<build>/server/wineserver test/pentouch-gate.sh
#   SG_COMPOSITOR_SRC=<sg-compositor checkout> (default: beside this repo)
#
# Mutants (each must fail it): SG_MUTANT_NO_PEN_POINTER (winex11 mouse.c),
# SG_MUTANT_NO_POINTER_INFO (win32u message.c), SG_MUTANT_NO_PEN_QUEUE
# (server queue.c), SG_MUTANT_NO_GESTURE (win32u input.c, since 1150).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
SRC="${SG_COMPOSITOR_SRC:-$HERE/../../sg-compositor}"
[ -d "$SRC" ] || SRC="${SG_REAL_HOME:-$HOME}/Stained-Glass-OS/sg-compositor"
RC=0; CP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for need in Xwayland meson ninja "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
grep -q '"pressure %lf"' "$SRC/seat.c" 2>/dev/null || { echo "SKIP: no sg-compositor 0.2.0+sg39 or later at $SRC"; exit 77; }

T=$(mktemp -d /var/tmp/sg-pentouch.XXXXXX)
stop() { [ -n "$CP" ] && kill -9 "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; CP=""; }
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    stop; rm -rf "$T"
}
trap cleanup EXIT INT TERM

meson setup "$T/comp" "$SRC" -Dtest-tablet=true -Dman-pages=disabled --buildtype=release >"$T/meson.log" 2>&1 &&
    ninja -C "$T/comp" >"$T/ninja.log" 2>&1 || { cat "$T/meson.log" "$T/ninja.log" | tail -20; echo "SKIP: cannot build the test compositor"; exit 77; }
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/penpointer-probe.exe" "$HERE/penpointer-probe.c" -luser32 -lgdi32 ||
    { fail "probe did not build"; exit 1; }

unset DISPLAY WAYLAND_DISPLAY
mkfifo "$T/pen"
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER

# start: a headless compositor (1280x720) whose child only says its X display
start() {
    rm -f "$T/disp"
    SG_TEST_TABLET_FIFO="$T/pen" WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman \
        "$T/comp/sg-compositor" -L "$T/priv.sock" -C "$T/ctl.sock" -U "$(id -u)" -- \
        sh -c "echo \$DISPLAY > $T/disp; exec sleep 900" >"$T/comp.log" 2>&1 &
    CP=$!
    i=0; while [ ! -s "$T/disp" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
    DISPLAY=$(cat "$T/disp" 2>/dev/null); export DISPLAY
    [ -n "$DISPLAY" ] && [ "$DISPLAY" != ":0" ] || { echo "no display from the compositor"; cat "$T/comp.log"; exit 1; }
    sleep 1
}
# probe LOG [ENV=VALUE...]: the drawing program, until it is ready
probe() {
    log=$1; shift
    env "$@" timeout -s KILL 300 "$WINE" "$T/penpointer-probe.exe" > "$log" 2>/dev/null &
    i=0; while ! grep -q '^ready' "$log" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.5; i=$((i + 1)); done
    grep -q '^ready' "$log" || { fail "the probe did not start: $(head -c 300 "$log")"; }
    sleep 1
}
# a real device's events come in frames of their own: one line per write
feed() { for l in "$@"; do printf '%s\n' "$l" > "$T/pen"; sleep 0.25; done; sleep 0.5; }
stroke() { feed "in 0.5 0.5" "move 0.5 0.5" "tilt 20 -10" "pressure 0.1" down "pressure 0.7" "move 0.51 0.5" \
                "pressure 0.3" "move 0.52 0.5" up "button 331 1" "button 331 0" out; }
finish() { "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null; stop; }

start
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w

# --- 1000: a window with a Wintab context, the pen over it ---
L=$T/windowed.log
probe "$L" PENPROBE_WINDOWED=1
stroke
finish
grep -q '^devices count=[1-9][0-9]* type=1' "$L" && pass "GetPointerDevices lists the pen ($(grep '^devices' "$L"))" \
    || fail "GetPointerDevices: $(grep '^devices' "$L")"
grep '^pointer down' "$L" | grep -q 'type=3 ok=1 pressure=102 ' \
    && pass "the tip is WM_POINTERDOWN of a pen (PT_PEN), pressure 0.1 = 102 of 1024" \
    || fail "no pen WM_POINTERDOWN with pressure 102: $(grep '^pointer down' "$L" | head -2)"
grep '^pointer update' "$L" | grep -q 'type=3 ok=1 pressure=717 tiltx=20 tilty=-10 ' \
    && pass "GetPointerPenInfo: pressure 0.7 = 717, tilt 20/-10" \
    || fail "no pressure 717 and tilt 20/-10: $(grep -o 'pressure=[0-9]* tiltx=[0-9-]* tilty=[0-9-]*' "$L" | sort -u | tr '\n' ' ')"
grep '^pointer update' "$L" | grep -q 'pressure=307 ' && pass "a lighter touch is lighter (0.3 = 307)" || fail "pressure 307 not seen"
grep '^pointer' "$L" | grep -q 'penmask=0xd ' && pass "the pen has pressure and tilt (PEN_MASK 0xd)" || fail "pen mask: $(grep -o 'penmask=[0-9a-fx]*' "$L" | sort -u)"
grep '^pointer up' "$L" | grep -q 'flags=0x42002 ' && pass "lifting it is WM_POINTERUP (POINTER_FLAG_UP, in range)" \
    || fail "WM_POINTERUP: $(grep '^pointer up' "$L" | head -1)"
grep '^pointer' "$L" | grep -q 'penflags=0x1 .*flags=0x22022 ' && pass "the barrel button: PEN_FLAG_BARREL, the second button" \
    || fail "no barrel button: $(grep -o 'penflags=[0-9a-fx]* penmask=[0-9a-fx]* flags=[0-9a-fx]*' "$L" | sort -u | tr '\n' ' ')"
grep '^pointer' "$L" | grep -q 'history=1 rects=1 ' && pass "GetPointerPenInfoHistory and GetPointerDeviceRects answer" \
    || fail "history/rects: $(grep -o 'history=[0-9]* rects=[0-9]*' "$L" | sort -u)"
grep -q '^mouse ldown extra=0xff515700' "$L" && pass "the pen's click is a mouse click marked as a pen's (MI_WP_SIGNATURE)" \
    || fail "the pen's click: $(grep '^mouse ldown' "$L" | head -1)"
grep -q '^mouse rdown' "$L" && pass "the barrel button right-clicks" || fail "no right click from the barrel"
grep -q '^wintab packet pressure=45874' "$L" && pass "Wintab's packets too, with the pen's pressure (45874 of 65535)" \
    || fail "Wintab: $(grep '^wintab' "$L" | sort | uniq -c | head -4 | tr '\n' ' ')"

# --- 1000: a full-screen program (its cursor clipping grabs the pointer) ---
start
L=$T/full.log
probe "$L" PENPROBE_NOWINTAB=1
stroke
finish
grep '^pointer update' "$L" | grep -q 'type=3 ok=1 pressure=717 tiltx=20 tilty=-10 ' \
    && pass "a full-screen program gets the pen's pressure and tilt too" \
    || fail "full screen: $(grep -c '^pointer' "$L") pointer messages, $(grep -o 'pressure=[0-9]*' "$L" | sort -u | tr '\n' ' ')"

# --- 1001: one finger clicks, two scroll, a pinch zooms ---
start
L=$T/touch.log
probe "$L" PENPROBE_WINDOWED=1 PENPROBE_NOWINTAB=1 PENPROBE_TOUCH_DEFPROC=1
feed "tdown 1 0.5 0.5" "tup 1"
cp "$L" "$T/tap.log"
steps=""
for k in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
    y=$(awk -v k="$k" 'BEGIN { printf "%.3f", 0.3 + k * 0.02 }'); steps="$steps|tmove 1 0.45 $y|tmove 2 0.55 $y"
done
feed "tdown 1 0.45 0.3" "tdown 2 0.55 0.3"
IFS='|'; set -- $steps; unset IFS; shift; feed "$@"
feed "tup 1" "tup 2"
cp "$L" "$T/pan.log"
steps=""
for k in 1 2 3 4 5 6 7 8 9 10; do
    a=$(awk -v k="$k" 'BEGIN { printf "%.3f", 0.45 - k * 0.02 }'); b=$(awk -v k="$k" 'BEGIN { printf "%.3f", 0.55 + k * 0.02 }')
    steps="$steps|tmove 1 $a 0.5|tmove 2 $b 0.5"
done
feed "tdown 1 0.45 0.5" "tdown 2 0.55 0.5"
IFS='|'; set -- $steps; unset IFS; shift; feed "$@"
feed "tup 1" "tup 2"
finish
grep -q '^mouse ldown' "$T/tap.log" && grep -q '^mouse lup' "$T/tap.log" && pass "a tap is a click (the server's promotion of a touch, wine-sg 1150)" \
    || fail "a tap did not click: $(grep -c '^mouse' "$T/tap.log") mouse messages"
n=$(grep -c '^wheel delta=120.\?$' "$T/pan.log")
[ "$n" -ge 3 ] && pass "two fingers moving down scroll the page up: $n wheel notches" \
    || fail "two-finger scroll: $n wheel notches ($(grep -c wheel "$T/pan.log") wheel messages)"
grep -q '^wheel delta=-' "$T/pan.log" && fail "the scroll went the wrong way" || pass "every notch the fingers' way"
n=$(sed -n "$(($(wc -l < "$T/pan.log") + 1)),\$p" "$L" | grep -c '^ctrl-wheel delta=120.\?$')
[ "$n" -ge 3 ] && pass "spreading two fingers zooms in: $n Ctrl+wheel notches" \
    || fail "pinch: $n Ctrl+wheel notches"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
