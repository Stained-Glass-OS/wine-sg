#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A touch screen's touches reach Windows programs as touches (patches/sg/1150),
# through sg-compositor's test build (-Dtest-tablet=true: a pen and a touch
# screen fed from a FIFO through the real input path; Xwayland makes them X
# input devices). test/touch-probe.c is the program.
#
#  1. A program that takes touches (WM_POINTER*, as Chromium and Qt do) gets
#     WM_POINTERDOWN/UPDATE/UP of PT_TOUCH with GetPointerTouchInfo and the
#     frame of every finger down (GetPointerFrameTouchInfo), and no mouse
#     messages of them; GetSystemMetrics(SM_DIGITIZER) names a multi-touch
#     screen. Before: the X server made touches the mouse; no touch at all.
#  2. A window registered with RegisterTouchWindow gets WM_TOUCH
#     (GetTouchInputInfo).
#  3. A program that leaves pointer messages to DefWindowProc (most): a tap
#     clicks a button, a drag selects text and moves a window by its title
#     bar; its mouse messages are marked as a touch's (0xff51578x); two
#     fingers scroll (WM_GESTURE, then the wheel) and pinch zooms (Ctrl+
#     wheel), and a scroll or pinch starting over a window presses no mouse
#     button there (the first finger's press waits). All in the shell's own
#     desktop, as in a session.
#  4. A swipe in from the right edge is Win+A (the notification centre), from
#     the left Win+Tab (Task View).
#  5. A pen first seen after the program started reaches it (an X device made
#     later: XI_HierarchyChanged); a pen lifted out of range leaves its window
#     (WM_POINTERLEAVE; sg-compositor's _SG_PEN_IN_RANGE).
#
#   WINE=<build>/wine WINESERVER=<build>/server/wineserver test/touch-gate.sh
#   SG_COMPOSITOR_SRC=<sg-compositor checkout, 0.2.0+sg40 or later>
#
# Mutants (each must fail it): SG_MUTANT_NO_TOUCH_POINTER, NO_PEN_HOTPLUG,
# NO_PEN_LEAVE, NO_EDGE_SWIPE, PEN_SIGNED_LPARAM (winex11 mouse.c);
# TOUCH_GRAB (winex11 window.c); NO_TOUCH_PROMOTE, TOUCH_PRESS_AT_ONCE,
# NO_PEN_LEAVE_QUEUED (server queue.c); NO_TOUCH_INFO (win32u message.c);
# TOUCH_MOUSE_ALWAYS, NO_GESTURE, NO_TOUCH_ACTIVATE, NO_CTRL_WAIT,
# GESTURE_END_WHEEL (win32u input.c); NO_WM_TOUCH (user32 input.c);
# POINTERDOWN_UNWINDABLE (user32 message.c). TOUCH_SECTIONS=135 runs only
# those sections; TOUCH_KEEP=1 keeps the logs.
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

for need in Xwayland meson ninja awk "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
grep -q 'SG_TEST_TABLET_LATE' "$SRC/seat.c" 2>/dev/null || { echo "SKIP: no sg-compositor 0.2.0+sg40 or later at $SRC"; exit 77; }

T=$(mktemp -d /var/tmp/sg-touch.XXXXXX)
stop() { [ -n "$CP" ] && kill -9 "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; CP=""; }
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    stop; [ -n "${TOUCH_KEEP:-}" ] && echo "kept $T" || rm -rf "$T"
}
trap cleanup EXIT INT TERM

if [ -n "${SG_COMPOSITOR_BUILD:-}" ] && [ -x "$SG_COMPOSITOR_BUILD/sg-compositor" ]; then
    COMP=$SG_COMPOSITOR_BUILD/sg-compositor
else
    meson setup "$T/comp" "$SRC" -Dtest-tablet=true -Dman-pages=disabled --buildtype=release >"$T/meson.log" 2>&1 &&
        ninja -C "$T/comp" >"$T/ninja.log" 2>&1 || { tail -20 "$T/meson.log" "$T/ninja.log"; echo "SKIP: cannot build the test compositor"; exit 77; }
    COMP=$T/comp/sg-compositor
fi
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/touch-probe.exe" "$HERE/touch-probe.c" -luser32 -lgdi32 ||
    { fail "probe did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/penpointer-probe.exe" "$HERE/penpointer-probe.c" -luser32 -lgdi32 ||
    { fail "pen probe did not build"; exit 1; }

unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
mkfifo "$T/pen"
mkdir -p "$T/prefix" "$T/comp-xdg" "$T/wine-xdg"
chmod 700 "$T/comp-xdg" "$T/wine-xdg"
# Wine's programs find no Wayland socket: a program there (wineboot's window)
# would be a surface the touches land on instead of the X ones
export XDG_RUNTIME_DIR="$T/wine-xdg"
export WINEPREFIX="$T/prefix" WINEDEBUG="${TOUCH_DEBUG:--all}" WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER

# start [ENV=VALUE...]: a headless compositor (1280x720) whose child only says its X display
start() {
    rm -f "$T/disp"
    env "$@" XDG_RUNTIME_DIR="$T/comp-xdg" SG_TEST_TABLET_FIFO="$T/pen" WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman \
        "$COMP" -L "$T/priv.sock" -C "$T/ctl.sock" -U "$(id -u)" -- \
        sh -c "echo \$DISPLAY > $T/disp; exec sleep 1200" >>"$T/comp.log" 2>&1 &
    CP=$!
    i=0; while [ ! -s "$T/disp" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
    DISPLAY=$(cat "$T/disp" 2>/dev/null); export DISPLAY
    [ -n "$DISPLAY" ] && [ "$DISPLAY" != ":0" ] || { echo "no display from the compositor"; cat "$T/comp.log"; exit 1; }
    sleep 1
}
finish() { "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null; stop; }
# probe LOG EXE ARGS...: a program, until it says "ready"
probe() {
    log=$1; shift
    timeout -s KILL 600 "$WINE" "$@" > "$log" 2>"$log.err" &
    i=0; while ! grep -q '^ready' "$log" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.5; i=$((i + 1)); done
    grep -q '^ready' "$log" || fail "$1 did not start: $(head -c 300 "$log") $(tail -c 300 "$log.err")"
    sleep 1
}
# a real device's events come in frames of their own: one line per write;
# "a|b" writes a and b at once (two fingers landing together)
feed() { for l in "$@"; do printf '%s\n' "$l" | tr '|' '\n' > "$T/pen"; sleep 0.25; done; sleep 0.6; }
# fx X, fy Y: screen pixels as the FIFO's fractions of 1280x720
fx() { awk -v v="$1" 'BEGIN { printf "%.4f", v / 1280 }'; }
fy() { awk -v v="$1" 'BEGIN { printf "%.4f", v / 720 }'; }
mark() { wc -l < "$1" > "$T/mark"; }
since() { tail -n +"$(( $(cat "$T/mark") + 1 ))" "$1" | tr -d '\r'; }
rect() { tr -d '\r' < "$1" | grep "^rect $2 " | head -1 | cut -d' ' -f3-; }

# TOUCH_SECTIONS: which sections to run (default all: 1234... in their order)
want() { case "${TOUCH_SECTIONS:-12345}" in *"$1"*) return 0;; esac; return 1; }

start
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
# X11 only: a Wine program on the compositor's Wayland socket (wineboot's
# window, once) is a surface the touches land on instead
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d x11 /f >/dev/null 2>&1
"$WINESERVER" -w
finish

# --- 1. a program that takes touches ---
if want 1 || want 4; then
start
L=$T/aware.log
probe "$L" "$T/touch-probe.exe" aware
d=$(sed -n 's/^metrics digitizer=\(0x[0-9a-f]*\).*/\1/p' "$L")
[ $(( ${d:-0} & 0xc1 )) = $((0xc1)) ] && grep -q 'touchdev=[1-9]' "$L" \
    && pass "a multi-touch screen: SM_DIGITIZER $d, GetPointerDevices lists it ($(grep '^metrics' "$L"))" \
    || fail "no touch screen for programs: $(grep '^metrics' "$L")"
mark "$L"
feed "tdown 1 0.5 0.5" "tmove 1 0.52 0.5" "tup 1"
since "$L" > "$T/tap1"
grep -q '^pointer down id=[0-9]* type=2 touchok=1 frame=1 .*primary=1' "$T/tap1" \
    && pass "a touch is WM_POINTERDOWN of PT_TOUCH, GetPointerTouchInfo answers, a frame of one" \
    || fail "no touch WM_POINTERDOWN: $(grep -c '^pointer' "$T/tap1") pointer messages; $(grep '^pointer' "$T/tap1" | head -2 | tr '\n' ' ')"
grep -q '^pointer update .*type=2 touchok=1' "$T/tap1" && grep -q '^pointer up .*type=2' "$T/tap1" \
    && pass "it moves (WM_POINTERUPDATE) and lifts (WM_POINTERUP)" || fail "no update/up: $(grep '^pointer' "$T/tap1" | cut -c1-40 | sort | uniq -c | tr '\n' ' ')"
x=$(grep '^pointer down' "$T/tap1" | head -1 | sed 's/.* x=\([0-9-]*\) .*/\1/')
[ "${x:-0}" -ge 630 ] && [ "${x:-0}" -le 650 ] && pass "where it is: x=$x of 640" || fail "the touch's place: x=${x:-none}, not 640"
grep -q '^pointer down .*stamp=1' "$T/tap1" && pass "the touch is stamped on the desktop for the touch keyboard (__wine_sg_touch_time)" \
    || fail "no touch time on the desktop window"
grep -q '^mouse l' "$T/tap1" && fail "the program that took the touch got mouse clicks too: $(grep '^mouse' "$T/tap1" | head -2 | tr '\n' ' ')" \
    || pass "and no mouse button messages of it"
mark "$L"
feed "tdown 1 0.4 0.5" "tdown 2 0.6 0.5" "tmove 2 0.62 0.5" "tup 2" "tup 1"
since "$L" > "$T/two"
grep -q '^pointer down .*type=2 touchok=1 frame=2 .*primary=0' "$T/two" \
    && pass "a second finger: its own pointer, not primary, the frame holds both (GetPointerFrameTouchInfo)" \
    || fail "two fingers: $(grep '^pointer down' "$T/two" | tr '\n' ' ')"

# --- 4. edge swipes (the probe holds the shell's keys here) ---
if grep -q '^hotkeys 1 1' "$L"; then
    mark "$L"
    feed "tdown 5 0.999 0.5" "tmove 5 0.97 0.5" "tmove 5 0.92 0.5" "tmove 5 0.85 0.51" "tup 5"
    since "$L" | grep -q '^hotkey notify' && pass "a swipe in from the right edge is Win+A (the notification centre)" \
        || fail "a swipe from the right: $(since "$L" | grep -c hotkey) hotkeys"
    mark "$L"
    feed "tdown 6 0.0 0.5" "tmove 6 0.03 0.5" "tmove 6 0.08 0.5" "tmove 6 0.15 0.49" "tup 6"
    since "$L" | grep -q '^hotkey taskview' && pass "a swipe in from the left edge is Win+Tab (Task View)" \
        || fail "a swipe from the left: $(since "$L" | grep -c hotkey) hotkeys"
    mark "$L"
    feed "tdown 7 0.5 0.3" "tmove 7 0.6 0.3" "tmove 7 0.7 0.3" "tup 7"
    since "$L" | grep -q '^hotkey' && fail "a swipe in the middle opened something" || pass "a swipe away from the edges is no shell gesture"
else
    fail "the probe could not hold Win+Tab and Win+A: $(grep '^hotkeys' "$L")"
fi
finish
fi

# --- 2. WM_TOUCH ---
if want 2; then
start
L=$T/touchwin.log
probe "$L" "$T/touch-probe.exe" touchwin
grep -q '^registered=1 istouch=1' "$L" && pass "RegisterTouchWindow, IsTouchWindow" || fail "RegisterTouchWindow: $(grep '^registered' "$L")"
mark "$L"
feed "tdown 1 0.5 0.5" "tdown 2 0.6 0.5" "tmove 2 0.65 0.5" "tup 2" "tup 1"
since "$L" > "$T/wmtouch"
grep -q '^touch count=1 down=1 up=0' "$T/wmtouch" && grep -q '^touch count=2 down=1 up=0' "$T/wmtouch" \
    && grep -q '^touch count=[12] down=0 up=1' "$T/wmtouch" \
    && pass "WM_TOUCH: each finger down and up, every finger in each (GetTouchInputInfo)" \
    || fail "WM_TOUCH: $(sort "$T/wmtouch" | uniq -c | head -8 | tr '\n' ' ')"
grep -q '^gesture' "$T/wmtouch" && fail "a touch window got WM_GESTURE too" || pass "and no WM_GESTURE (a touch window's own)"
finish
fi

# --- 3. a program leaving touches to DefWindowProc, in the shell's desktop ---
if want 3; then
start
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x720 /f >/dev/null 2>&1
"$WINESERVER" -w
WINEDEBUG="${TOUCH_DEBUG:-err+all}" "$WINE" explorer /desktop=shell,1280x720 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 3
L=$T/unaware.log
probe "$L" "$T/touch-probe.exe" unaware
set -- $(rect "$L" button); bx=$(( ($1 + $3) / 2 )); by=$(( ($2 + $4) / 2 ))
set -- $(rect "$L" edit); el=$1; er=$3; ey=$(( ($2 + $4) / 2 ))
set -- $(rect "$L" window); wl=$1; wt=$2; wr=$3
set -- $(rect "$L" client); cl=$1; ct=$2; cr=$3; cb=$4
# a tap on the button
mark "$L"
feed "tdown 1 $(fx $bx) $(fy $by)" "tup 1"
since "$L" | grep -q '^clicked button' && pass "a tap clicks a button" || fail "a tap on the button did not click: $(since "$L" | head -5 | tr '\n' ' ')"
# a tap on the window itself: marked as a touch's
mark "$L"
px=$(( (cl + cr) / 2 )); py=$(( cb - 40 ))
feed "tdown 1 $(fx $px) $(fy $py)" "tup 1"
since "$L" | grep -q '^mouse ldown extra=0xff51578[0-9a-f]' && since "$L" | grep -q '^mouse lup extra=0xff51578' \
    && pass "its mouse messages are a touch's (GetMessageExtraInfo $(since "$L" | grep -o 'ldown extra=[0-9a-fx]*' | head -1))" \
    || fail "the tap's mouse messages: $(since "$L" | grep '^mouse' | head -3 | tr '\n' ' ')"
since "$L" | grep -q '^pointer down .*type=2 touchok=1' && pass "after its WM_POINTERDOWN of PT_TOUCH, left to DefWindowProc" \
    || fail "no touch WM_POINTERDOWN for the window leaving it to DefWindowProc"
# a drag across the edit's text selects it
mark "$L"
steps=""; k=0
while [ $k -le 8 ]; do steps="$steps|tmove 1 $(fx $(( el + 8 + (er - el - 16) * k / 8 ))) $(fy $ey)"; k=$((k + 1)); done
IFS='|'; set -- $steps; unset IFS; shift
feed "tdown 1 $(fx $(( el + 6 ))) $(fy $ey)" "$@" "tup 1"
sel=$(since "$L" | grep '^sel' | tail -1)
set -- $sel
[ -n "$sel" ] && [ "$3" -gt $(( $2 + 10 )) ] && pass "a drag across a text field selects its text ($sel)" || fail "a drag in the edit: ${sel:-no selection}"
# a drag on the title bar moves the window
mark "$L"
cy=$(( wt + (ct - wt) / 2 )); cx=$(( (wl + wr) / 2 ))
feed "tdown 1 $(fx $cx) $(fy $cy)" "tmove 1 $(fx $(( cx + 30 ))) $(fy $cy)" "tmove 1 $(fx $(( cx + 60 ))) $(fy $(( cy + 20 )))" \
     "tmove 1 $(fx $(( cx + 100 ))) $(fy $(( cy + 40 )))" "tup 1"
sleep 0.5
pos=$(since "$L" | grep '^pos' | tail -1)
set -- $pos
[ -n "$pos" ] && [ "$2" -ge $(( wl + 80 )) ] && [ "$3" -ge $(( wt + 25 )) ] \
    && pass "a drag on the title bar moves the window ($wl,$wt -> $2,$3)" || fail "the title bar drag: ${pos:-did not move} (from $wl,$wt)"
set -- $(tr -d '\r' < "$L" | grep '^pos' | tail -1); wl=$2; wt=$3
set -- $(rect "$L" button)
# the window moved: the button with it
dx=$(( wl - $(rect "$L" window | cut -d' ' -f1) )); dy=$(( wt - $(rect "$L" window | cut -d' ' -f2) ))
bx=$(( bx + dx )); by=$(( by + dy )); px=$(( px + dx )); py=$(( py + dy ))
# two fingers over the button scroll: no click, the wheel
mark "$L"
steps=""; k=1
while [ $k -le 12 ]; do steps="$steps|tmove 1 $(fx $bx) $(fy $(( by + k * 18 )))|tmove 2 $(fx $(( bx + 60 ))) $(fy $(( by + k * 18 )))"; k=$((k + 1)); done
IFS='|'; set -- $steps; unset IFS; shift
feed "tdown 1 $(fx $bx) $(fy $by)|tdown 2 $(fx $(( bx + 60 ))) $(fy $by)" "$@" "tup 1|tup 2"
since "$L" > "$T/scroll"
n=$(grep -c '^wheel delta=120' "$T/scroll")
[ "$n" -ge 3 ] && pass "two fingers moving down scroll the page up: $n wheel notches" \
    || fail "two-finger scroll: $n wheel notches; $(cut -c1-30 "$T/scroll" | sort | uniq -c | head -8 | tr '\n' ' ')"
grep -q '^wheel delta=-' "$T/scroll" && fail "the scroll went the wrong way" || pass "every notch the fingers' way"
grep -q '^clicked button' "$T/scroll" && fail "a scroll starting over the button clicked it" || pass "a scroll starting over a button does not click it"
# two fingers sideways on the window itself: WM_GESTURE's pan, then the
# horizontal wheel
mark "$L"
steps=""; k=1
while [ $k -le 10 ]; do steps="$steps|tmove 1 $(fx $(( px - 100 - k * 20 ))) $(fy $py)|tmove 2 $(fx $(( px - 40 - k * 20 ))) $(fy $py)"; k=$((k + 1)); done
IFS='|'; set -- $steps; unset IFS; shift
feed "tdown 1 $(fx $(( px - 100 ))) $(fy $py)|tdown 2 $(fx $(( px - 40 ))) $(fy $py)" "$@" "tup 1|tup 2"
since "$L" > "$T/pan"
grep -q '^gesture id=1 ok=1 flags=0x1' "$T/pan" && grep -q '^gesture id=4 ok=1' "$T/pan" && grep -q '^gesture id=4 ok=1 flags=0x4' "$T/pan" \
    && grep -q '^gesture id=2 ok=1' "$T/pan" \
    && pass "WM_GESTURE: begin, pan, its end, end (GetGestureInfo), to the window under the fingers" \
    || fail "WM_GESTURE of a pan: $(grep '^gesture' "$T/pan" | cut -c1-28 | sort | uniq -c | tr '\n' ' ')"
n=$(grep -c '^hwheel delta=120' "$T/pan")
[ "$n" -ge 3 ] && pass "then DefWindowProc's wheel: fingers moving left are $n notches of the horizontal wheel" \
    || fail "a sideways pan: $n horizontal wheel notches; $(cut -c1-30 "$T/pan" | sort | uniq -c | head -8 | tr '\n' ' ')"
# two fingers on the window itself: its mouse button is never pressed
mark "$L"
steps=""; k=1
while [ $k -le 10 ]; do steps="$steps|tmove 1 $(fx $(( px - 40 - k * 12 ))) $(fy $py)|tmove 2 $(fx $(( px + 40 + k * 12 ))) $(fy $py)"; k=$((k + 1)); done
IFS='|'; set -- $steps; unset IFS; shift
feed "tdown 1 $(fx $(( px - 40 ))) $(fy $py)|tdown 2 $(fx $(( px + 40 ))) $(fy $py)" "$@" "tup 1|tup 2"
since "$L" > "$T/pinch"
grep -q '^gesture id=3 ok=1' "$T/pinch" && pass "a pinch is WM_GESTURE's zoom" \
    || fail "WM_GESTURE of a pinch: $(grep '^gesture' "$T/pinch" | cut -c1-28 | sort | uniq -c | tr '\n' ' ')"
n=$(grep -c '^ctrl-wheel delta=120' "$T/pinch")
[ "$n" -ge 3 ] && pass "spreading two fingers zooms in: $n Ctrl+wheel notches" \
    || fail "pinch: $n Ctrl+wheel notches; $(cut -c1-30 "$T/pinch" | sort | uniq -c | head -8 | tr '\n' ' ')"
grep -q '^ctrl-wheel delta=-' "$T/pinch" && fail "the spreading fingers zoomed out too ($(grep -c '^ctrl-wheel delta=-' "$T/pinch") notches: the gesture's end)" \
    || pass "only in: its end zooms no more"
grep -q '^mouse ldown' "$T/pinch" && fail "the pinch pressed the window's mouse button: $(grep '^mouse ldown' "$T/pinch" | head -1)" \
    || pass "a pinch presses no mouse button (the first finger's press waits for the second)"
# a program taking touches itself, in the background: a touch on it brings
# it to the front (no mouse click is made of the touch to do it; in the
# shell's desktop the compositor's focus is the desktop's, not the program's)
"$WINE" "$T/touch-probe.exe" aware-small > "$T/small.log" 2>/dev/null &
i=0; while ! grep -q '^ready' "$T/small.log" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 1
feed "tdown 1 $(fx $px) $(fy $py)" "tup 1"
mark "$T/small.log"
set -- $(rect "$T/small.log" window)
feed "tdown 1 $(fx $(( ($1 + $3) / 2 ))) $(fy $(( ($2 + $4) / 2 )))" "tup 1"
since "$T/small.log" | grep -q '^activated' && pass "a touch brings the program it lands on to the front (WM_POINTERACTIVATE)" \
    || fail "the touched program stayed behind: $(since "$T/small.log" | grep -v update | head -3 | tr '\n' ' ')"
n=$(grep -c 'wheel' "$T/small.log")
[ "$n" = 0 ] && pass "and gestures ended when their fingers lifted: no wheel after them" \
    || fail "$n wheel messages after the gestures ended (a gesture's end made the wheel go on)"
finish
fi

# --- 5. a pen plugged in after the program started; a pen leaving range ---
if want 5; then
start SG_TEST_TABLET_LATE=1
L=$T/pen.log
export PENPROBE_WINDOWED=1 PENPROBE_NOWINTAB=1
probe "$L" "$T/penpointer-probe.exe"
unset PENPROBE_WINDOWED PENPROBE_NOWINTAB
grep -q '^devices count=[0-9]* .*types=\([0-9,]*,\)\?[12]\(,\|$\)' "$L" && fail "a pen before the tablet: $(grep '^devices' "$L")" \
    || pass "before the pen comes, no pen device ($(grep '^devices' "$L"))"
feed plug
sleep 2
mark "$L"
feed "in 0.5 0.5" "move 0.5 0.5" "pressure 0.1" down "pressure 0.7" "move 0.51 0.5" up "move 0.53 0.5" out
since "$L" > "$T/latepen"
grep '^pointer down' "$T/latepen" | grep -q 'type=3 ok=1 pressure=102 ' \
    && pass "a pen first seen after the program started reaches it (WM_POINTERDOWN, pressure 102)" \
    || fail "the late pen: $(grep -c '^pointer' "$T/latepen") pointer messages; $(head -3 "$T/latepen" | tr '\n' ' ')"
grep -q '^pointer leave' "$T/latepen" && pass "lifted out of range, the pen leaves the window (WM_POINTERLEAVE)" \
    || fail "no WM_POINTERLEAVE when the pen left: $(grep '^pointer' "$T/latepen" | cut -c1-15 | sort | uniq -c | tr '\n' ' ')"
f=$(grep '^pointer leave' "$T/latepen" | tail -1 | sed -n 's/.* flags=\(0x[0-9a-f]*\) .*/\1/p')
[ -n "$f" ] && [ $(( f & 2 )) = 0 ] && pass "no longer in range (POINTER_FLAG_INRANGE clear: $f)" || fail "the leave's flags: ${f:-none}"
# in the screen's lower half too (its place once spoiled the pen's state)
mark "$L"
feed "in 0.5 0.8" "move 0.5 0.8" "tilt 20 -10" "pressure 0.1" down "pressure 0.7" "move 0.51 0.8" up out
grep_low=$(since "$L" | grep '^pointer update' | grep -c 'type=3 ok=1 pressure=717 tiltx=20 tilty=-10 ')
[ "$grep_low" -ge 1 ] && pass "the pen's pressure and tilt in the screen's lower half" \
    || fail "the pen in the lower half: $(since "$L" | grep -o 'pressure=[0-9]* tiltx=[0-9-]* tilty=[0-9-]* penflags=[0-9a-fx]*' | sort -u | head -3 | tr '\n' ' ')"
finish
fi

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
