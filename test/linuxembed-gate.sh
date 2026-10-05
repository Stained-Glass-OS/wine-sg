#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program's window goes into a Wine window of its own (wine-sg
# patches/sg/0762, winex11 sg_embed.c): it stacks among Wine's windows and is
# activated, minimized and closed as they are (David 2026-10-02). A real
# xterm, listed to the taskbar by a stand-in sg-lockctl as the compositor
# lists it:
#   1. it is put into the taskbar's SgLinuxWindow (its X parent is Wine's)
#   2. a Wine window brought forward covers it, and it comes forward again
#      (X stacking, as drawn: the pixel where they overlap)
#   3. a click into it activates its frame; typed keys reach it once its
#      frame is active, the pointer elsewhere
#   4. Alt+Tab from it to Notepad and back: released, the switch is made
#      (the keyboard is grabbed on Tab, so Alt's release is the shell's) and
#      the window chosen comes forward
#   5. it asks for full screen (_NET_WM_STATE, as Firefox's F11 or a video):
#      its frame covers the screen, and is back where it was after; it is
#      in NormalState (WM_STATE), or GTK takes it for withdrawn and asks
#      nobody
#   5b. it asks to be maximized, and restored: its frame is; its frame
#      maximized, the program is told (_NET_WM_STATE); one maximized before
#      it was framed (Firefox, restored maximized) gets a maximized frame
#   5c. dragged to the screen's edge it snaps to that half; Win+Right too
#   5d. it asks to be minimized (its own minimize button: XIconifyWindow):
#      its frame is, and it is told (WM_STATE Iconic), and Normal again
#      when restored (0854)
#   5e. minimized, Notepad active, it asks to be the active window
#      (_NET_ACTIVE_WINDOW: gtk_window_present, Firefox given a link): its
#      frame is restored and active (0854)
#   4b. active, it is told it has the focus (_NET_WM_STATE_FOCUSED, which
#      GTK draws itself active or greyed by); Notepad active, not (0854)
#   6. it is on its own desktop: on the next one it is not shown
#   7. Task View's card for it shows it, not a black picture
#   8. WM_CLOSE to the frame (its close button, End task) closes it, and the
#      frame goes; a list older than its going (the compositor's, asked for
#      before) brings no stand-in or button back for it (0855)
#  10. a request about a window already gone does not end the shell
#  11. one opening while a Wine window is active is active and has the keys;
#      the window that was active is not made active again meanwhile (its
#      program, told late, took the X focus back: 0853)
#   9. a hung one (stopped) does not close when asked; Task Manager's End
#      task then (SgLinuxWindowEnd) ends its process (patches/sg/0772; CPU-X
#      could not be ended, David 2026-10-02)
#
#   WINE=/opt/wine-sg/bin/wine test/linuxembed-gate.sh
# Mutants (0853-0855, dlls/winex11.drv/sg_embed.c): SG_MUTANT_EMBED_REPARENT_HIDES
# fails 11 (the window behind made active while it is framed; on a busy
# machine the keys then go to it); SG_MUTANT_EMBED_NO_ICONIFY fails 5d;
# SG_MUTANT_EMBED_NO_FOCUSED_STATE fails 4b; SG_MUTANT_EMBED_NO_ACTIVE_WINDOW
# fails 5e; SG_MUTANT_GONE_XWIN_BUTTON (programs/explorer/systray.c) fails
# 8's "nothing comes back".
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
# xterm talks to the session manager the environment names (the real
# session's): unanswered, its window came seconds late on a busy machine
unset SESSION_MANAGER
for t in Xvfb xdotool xterm xwininfo xprop import convert cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-linuxembed.XXXXXX); XP=; XT=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XT" ] && kill "$XT" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/netwm-fullscreen" "$HERE/netwm-fullscreen.c" -lX11 || { echo "FAIL  netwm-fullscreen did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
xterm -geometry 50x12+120+120 -bg '#30a060' -fg white -title 'Linux Terminal' -e sh -c "cd '$T'; exec sh" & XT=$!
i=0; while [ -z "$(xwininfo -root -tree | awk '/"Linux Terminal"/ {print $1; exit}')" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
XID=$(xwininfo -root -tree | awk '/"Linux Terminal"/ {print $1; exit}')
printf '%d shown - XTerm\tLinux Terminal\nEND\n' "$XID" > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
WINEDEBUG="${EMBED_DEBUG:--all}" "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 5    # the taskbar reads the list
cd "$WINEPREFIX/drive_c"

# framed XID: wait until the window is in a Wine window (up to 20 s)
framed() {
    _k=0
    while [ $_k -lt 80 ]; do
        case "$(xwininfo -id "$1" -tree 2>/dev/null | sed -n 's/.*Parent window id: [^ ]* //p')" in
            ""|"(the root window)"*) ;; *) return 0 ;; esac
        sleep 0.25; _k=$((_k + 1))
    done
    return 1
}

# 1.
parent=$(xwininfo -id "$XID" -tree 2>/dev/null | awk '/Parent window id:/ {print $5}')
pname=$(xwininfo -id "$XID" -tree 2>/dev/null | sed -n 's/.*Parent window id: [^ ]* //p')
case "$pname" in *"explorer.exe"*|*"Linux Terminal"*) [ "$pname" != "(the root window)" ] && pass "the Linux window is in a Wine window ($parent)" ;;
    *) fail "the Linux window's parent: $pname" ;; esac

# its colour as drawn (xterm's -bg, as the server has it)
px() { import -window root -crop 1x1+260+230 -depth 8 txt:- 2>/dev/null | tail -1 | grep -o '#[0-9A-F]\{6\}'; }
COL=$(px)

# 2. Notepad over it, then it over Notepad: the pixel where they overlap
"$WINE" notepad > /dev/null 2>&1 &
sleep 4
"$WINE" probe.exe move Notepad "" 200 160 500 400 > /dev/null 2>&1
"$WINE" probe.exe activate Notepad "" > /dev/null 2>&1; sleep 1.5
p1=$(px)
"$WINE" probe.exe activate SgLinuxWindow "Linux Terminal" > /dev/null 2>&1; sleep 1.5
p2=$(px)
[ "$p1" != "$COL" ] && [ "$p2" = "$COL" ] && pass "it stacks among Wine's windows: under Notepad ($p1), then over it ($p2)" \
    || fail "stacking: with Notepad active $p1, with it active $p2 (want not, then $COL)"

# 2b. a click into it, Notepad active: its frame comes forward
"$WINE" probe.exe activate Notepad "" > /dev/null 2>&1; sleep 1
xdotool mousemove 150 150; sleep 0.3; xdotool click 1; sleep 1.5
"$WINE" probe.exe foreground SgLinuxWindow "Linux Terminal" | grep -q 'foreground=1' && [ "$(px)" = "$COL" ] \
    && "$WINE" probe.exe zabove SgLinuxWindow "Linux Terminal" Notepad "" | grep -q 'above=1' \
    && pass "a click into it brings its frame forward: active, and over Notepad ($(px))" \
    || fail "a click into it: active $("$WINE" probe.exe foreground SgLinuxWindow "Linux Terminal" | tr -d '\r'), where it overlaps Notepad $(px) (want $COL), $("$WINE" probe.exe zabove SgLinuxWindow "Linux Terminal" Notepad "" | tr -d '\r')"

# 3. keys, the pointer away from it
xdotool mousemove 900 650; sleep 0.3
xdotool type --delay 40 'touch typed-here'; xdotool key Return; sleep 1.5
[ -e "$T/typed-here" ] && pass "keys reach it when its frame is active" || fail "typed keys did not reach it"

# 4. Alt+Tab to Notepad, released; again, back to it (it comes forward)
fg() { "$WINE" probe.exe foreground "$1" "$2" | grep -q 'foreground=1'; }
alttab() { xdotool keydown alt; sleep 0.3; xdotool key Tab; sleep 1; xdotool keyup alt; sleep 1.5; }
# in front as drawn and in Wine's Z order, which the next Alt+Tab goes by
above() { "$WINE" probe.exe zabove "$1" "$2" "$3" "$4" | grep -q 'above=1'; }
alttab
fg Notepad "" && [ "$(px)" != "$COL" ] && above Notepad "" SgLinuxWindow "Linux Terminal" && a1=ok \
    || a1="Notepad foreground: $(fg Notepad "" && echo yes || echo no), pixel $(px), above it: $(above Notepad "" SgLinuxWindow "Linux Terminal" && echo yes || echo no)"
alttab
fg SgLinuxWindow "Linux Terminal" && [ "$(px)" = "$COL" ] && above SgLinuxWindow "Linux Terminal" Notepad "" && a2=ok \
    || a2="it foreground: $(fg SgLinuxWindow "Linux Terminal" && echo yes || echo no), pixel $(px), above Notepad: $(above SgLinuxWindow "Linux Terminal" Notepad "" && echo yes || echo no)"
[ "$a1" = ok ] && [ "$a2" = ok ] && pass "Alt+Tab from it to Notepad, and back: each released switch made, the window chosen in front" \
    || fail "Alt+Tab: to Notepad: $a1; back: $a2"

# 4b. _NET_WM_STATE_FOCUSED as it is active or not (Alt+Tab left it active)
netstate() { xprop -id "$XID" _NET_WM_STATE 2>/dev/null; }
f1=$(netstate)
"$WINE" probe.exe activate Notepad "" > /dev/null 2>&1; sleep 1.5; f2=$(netstate)
"$WINE" probe.exe activate SgLinuxWindow "Linux Terminal" > /dev/null 2>&1; sleep 1.5; f3=$(netstate)
case "$f1" in *_NET_WM_STATE_FOCUSED*) ok1=y ;; *) ok1=n ;; esac
case "$f2" in *_NET_WM_STATE_FOCUSED*) ok2=n ;; *) ok2=y ;; esac
case "$f3" in *_NET_WM_STATE_FOCUSED*) ok3=y ;; *) ok3=n ;; esac
[ "$ok1$ok2$ok3" = yyy ] && pass "it is told when it has the focus and when not (_NET_WM_STATE_FOCUSED)" \
    || fail "_NET_WM_STATE: active '$f1', Notepad active '$f2', active again '$f3' (want FOCUSED, not, FOCUSED)"

# 5. full screen, asked by the program, and out of it
"$WINE" probe.exe rect SgLinuxWindow "Linux Terminal" 2>/dev/null | tr -d '\r' > "$T/rect0"
"$T/netwm-fullscreen" "$XID" 1; sleep 2
"$WINE" probe.exe rect SgLinuxWindow "Linux Terminal" 2>/dev/null | tr -d '\r' > "$T/rect1"
"$T/netwm-fullscreen" "$XID" 0; sleep 2
"$WINE" probe.exe rect SgLinuxWindow "Linux Terminal" 2>/dev/null | tr -d '\r' > "$T/rect2"
[ "$(cat "$T/rect1")" = "0 0 1024 700" ] && [ "$(cat "$T/rect2")" = "$(cat "$T/rect0")" ] \
    && pass "full screen when it asks (0 0 1024 700), and back after ($(cat "$T/rect2"))" \
    || fail "full screen: before $(cat "$T/rect0"), in $(cat "$T/rect1"), out $(cat "$T/rect2")"
xprop -id "$XID" WM_STATE 2>/dev/null | grep -q 'window state: Normal' && pass "it is in NormalState (WM_STATE)" \
    || fail "WM_STATE: $(xprop -id "$XID" WM_STATE 2>&1 | tr '\n' ' ')"

# 5b. maximized: asked by the program, and by its frame
zoomed() { "$WINE" probe.exe zoomed SgLinuxWindow "$1" | grep -q 'zoomed=1'; }
"$T/netwm-fullscreen" "$XID" 1 maximize; sleep 2; z1=$(zoomed "Linux Terminal" && echo yes || echo no)
"$T/netwm-fullscreen" "$XID" 0 maximize; sleep 2; z2=$(zoomed "Linux Terminal" && echo yes || echo no)
"$WINE" probe.exe rect SgLinuxWindow "Linux Terminal" 2>/dev/null | tr -d '\r' > "$T/rect3"
[ "$z1" = yes ] && [ "$z2" = no ] && [ "$(cat "$T/rect3")" = "$(cat "$T/rect0")" ] \
    && pass "maximized when it asks, restored when it asks ($(cat "$T/rect3"))" || fail "maximize: asked $z1, restored $z2, at $(cat "$T/rect3")"
"$WINE" probe.exe maximize SgLinuxWindow "Linux Terminal" >/dev/null 2>&1; sleep 1.5
m1=$(xprop -id "$XID" _NET_WM_STATE 2>/dev/null)
"$WINE" probe.exe restore SgLinuxWindow "Linux Terminal" >/dev/null 2>&1; sleep 1.5
m2=$(xprop -id "$XID" _NET_WM_STATE 2>/dev/null)
case "$m1" in *_NET_WM_STATE_MAXIMIZED_VERT*_NET_WM_STATE_MAXIMIZED_HORZ*) case "$m2" in *MAXIMIZED*) ok=no ;; *) ok=yes ;; esac ;; *) ok=no ;; esac
[ "$ok" = yes ] && pass "its frame maximized and restored, the program is told (_NET_WM_STATE)" || fail "told: maximized '$m1', restored '$m2'"

# 5c. snapped as any window (David 2026-10-03: "if you drag the window to the
# edge it doesn't snap"): its title bar dragged to the left edge, held, let go
# -- the left half; Win+Right -- the right half (the frames are explorer's,
# and snapping took only other programs' windows)
"$WINE" probe.exe restore SgLinuxWindow "Linux Terminal" >/dev/null 2>&1
"$WINE" probe.exe activate SgLinuxWindow "Linux Terminal" > /dev/null 2>&1; sleep 1
# Win+Right first, on a window not snapped (on a left-snapped one it un-snaps, as on Windows)
read -r l t r b < "$T/rect0"
"$WINE" probe.exe move SgLinuxWindow "Linux Terminal" "$l" "$t" $((r - l)) $((b - t)) >/dev/null 2>&1; sleep 1
"$WINE" probe.exe activate SgLinuxWindow "Linux Terminal" > /dev/null 2>&1; sleep 1
xdotool key super+Right; sleep 1.5
"$WINE" probe.exe rect SgLinuxWindow "Linux Terminal" 2>/dev/null | tr -d '\r' > "$T/rect-winright"
read -r sl st sr sb < "$T/rect-winright"
[ "$sl" -ge 500 ] && [ "$sl" -le 524 ] && [ "$sr" -ge 1020 ] && [ "$sb" -ge 600 ] \
    && pass "Win+Right snaps it to the right half ($(cat "$T/rect-winright"))" \
    || fail "Win+Right: its frame at $(cat "$T/rect-winright") (want the right half)"
read -r l t r b < "$T/rect0"
"$WINE" probe.exe move SgLinuxWindow "Linux Terminal" "$l" "$t" $((r - l)) $((b - t)) >/dev/null 2>&1; sleep 1
"$WINE" probe.exe activate SgLinuxWindow "Linux Terminal" > /dev/null 2>&1; sleep 1
read -r l t r b < "$T/rect0"
tx=$(( (l + r) / 2 )); ty=$((t + 10))
xdotool mousemove "$tx" "$ty" mousedown 1; sleep 0.3
for x in $((tx - 40)) $((tx - 120)) 200 80 20 0; do xdotool mousemove "$x" "$ty"; sleep 0.15; done
sleep 0.8; xdotool mouseup 1; sleep 1.5
"$WINE" probe.exe rect SgLinuxWindow "Linux Terminal" 2>/dev/null | tr -d '\r' > "$T/rect-snap"
read -r sl st sr sb < "$T/rect-snap"
[ "$sl" -le 0 ] && [ "$st" -le 0 ] && [ "$sr" -ge 500 ] && [ "$sr" -le 524 ] && [ "$sb" -ge 600 ] \
    && pass "dragged to the left edge, it snaps to the left half ($(cat "$T/rect-snap"))" \
    || fail "drag to the edge: its frame at $(cat "$T/rect-snap") (want the left half)"
read -r l t r b < "$T/rect0"
"$WINE" probe.exe move SgLinuxWindow "Linux Terminal" "$l" "$t" $((r - l)) $((b - t)) >/dev/null 2>&1; sleep 1

# 5d. its own minimize button (XIconifyWindow)
"$WINE" probe.exe activate SgLinuxWindow "Linux Terminal" > /dev/null 2>&1; sleep 1
"$T/netwm-fullscreen" "$XID" iconify; sleep 2
i1=$("$WINE" probe.exe iconic SgLinuxWindow "Linux Terminal" | tr -d '\r'); w1=$(xprop -id "$XID" WM_STATE 2>/dev/null | sed -n 's/.*window state: //p')
"$WINE" probe.exe activate SgLinuxWindow "Linux Terminal" > /dev/null 2>&1; sleep 1.5
i2=$("$WINE" probe.exe iconic SgLinuxWindow "Linux Terminal" | tr -d '\r'); w2=$(xprop -id "$XID" WM_STATE 2>/dev/null | sed -n 's/.*window state: //p')
[ "$i1" = iconic=1 ] && [ "$w1" = Iconic ] && [ "$i2" = iconic=0 ] && [ "$w2" = Normal ] \
    && pass "its own minimize request minimizes its frame (WM_STATE Iconic), restored it is Normal" \
    || fail "minimize asked by it: $i1 ($w1), restored $i2 ($w2) (want iconic=1 Iconic, iconic=0 Normal)"
# 5e. minimized, Notepad active: it asks to be the active window
"$WINE" probe.exe minimize SgLinuxWindow "Linux Terminal" > /dev/null 2>&1; sleep 1
"$WINE" probe.exe activate Notepad "" > /dev/null 2>&1; sleep 1
"$T/netwm-fullscreen" "$XID" activate; sleep 2
i3=$("$WINE" probe.exe iconic SgLinuxWindow "Linux Terminal" | tr -d '\r')
fg SgLinuxWindow "Linux Terminal" && a3=active || a3="not active"
[ "$i3" = iconic=0 ] && [ "$a3" = active ] && pass "minimized, it asks to be the active window (_NET_ACTIVE_WINDOW): restored and active" \
    || fail "_NET_ACTIVE_WINDOW from it, minimized: $i3, $a3 (want restored and active)"
read -r l t r b < "$T/rect0"
"$WINE" probe.exe move SgLinuxWindow "Linux Terminal" "$l" "$t" $((r - l)) $((b - t)) >/dev/null 2>&1; sleep 1

# 6. the next desktop: it is not there; back, it is
"$WINE" probe.exe activate SgLinuxWindow "Linux Terminal" > /dev/null 2>&1; sleep 1
cpx() { import -window root -crop 1x1+150+150 -depth 8 txt:- 2>/dev/null | tail -1 | grep -o '#[0-9A-F]\{6\}'; }
c0=$(cpx); xdotool key ctrl+super+Right; sleep 2; c1=$(cpx); xdotool key ctrl+super+Left; sleep 2; c2=$(cpx)
[ "$c0" = "$COL" ] && [ "$c1" != "$COL" ] && [ "$c2" = "$COL" ] \
    && pass "it is on its own desktop: not on the next ($c1), there again on its own" \
    || fail "desktops: on its own $c0, on the next $c1, back $c2 (want $COL, not, $COL)"

# 7. Task View: its card shows it (its colour), not black
"$WINE" probe.exe activate SgLinuxWindow "Linux Terminal" > /dev/null 2>&1; sleep 1
xdotool key super+Tab; sleep 2.5
import -window root "$T/taskview.png" 2>/dev/null
xdotool key Escape; sleep 1
n=$(convert "$T/taskview.png" -fuzz 4% -fill black +opaque "$COL" -fill white -opaque "$COL" -format '%[fx:int(mean*w*h)]' info: 2>/dev/null)
[ "${n:-0}" -gt 2000 ] && pass "Task View shows it in its card ($n pixels of its colour)" \
    || fail "Task View: ${n:-no} pixels of its colour (its card black or missing)"

# 8. its close button (a click on the frame, not into the program)
"$WINE" probe.exe rect SgLinuxWindow "Linux Terminal" > "$T/rect" 2>/dev/null
set -- $(cat "$T/rect")
if [ $# = 4 ]; then
    xdotool mousemove $(( $3 - 18 )) $(( $2 + 14 )); sleep 0.5; xdotool click 1
    i=0; while kill -0 "$XT" 2>/dev/null && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
fi
kill -0 "$XT" 2>/dev/null && fail "its frame's close button did not close it" || { pass "its frame's close button closes it"; XT=; }
# the compositor's list read next may be older than its going (asked for
# before), still naming it: no taskbar button, no stand-in comes back for it
"$WINE" probe.exe watch SgLinuxWindow 3 > "$T/watch" 2>/dev/null &
i=0; while ! grep -q watching "$T/watch" 2>/dev/null && [ $i -lt 40 ]; do sleep 0.1; i=$((i + 1)); done
sleep 0.7
# gone from the compositor's list too, as the compositor's own list is at
# once: the next xterm's window may get the same X id, and a stale list
# naming it had the taskbar frame it at once (0803 reads the list quickly)
printf 'END\n' > "$T/list"
i=0; while ! grep -q seen= "$T/watch" 2>/dev/null && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
grep -q 'seen=0' "$T/watch" && pass "...and nothing comes back for it from a list older than its going" \
    || fail "after it closed a window came back for it from an older list ($(tr -d '\r' < "$T/watch" | tail -1); a taskbar button flickered)"
sleep 1.5
xterm -geometry 50x12+120+120 -bg black -fg white -title 'Linux Terminal' & XT=$!
i=0; while [ -z "$(xwininfo -root -children | awk '/"Linux Terminal"/ {print $1; exit}')" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
X2=$(xwininfo -root -children | awk '/"Linux Terminal"/ {print $1; exit}')
"$T/netwm-fullscreen" "$X2" preset-maximized   # as the compositor's window manager leaves one it maximized
printf '%d shown - XTerm\tLinux Terminal\nEND\n' "$X2" > "$T/list"
sleep 4   # framed at the next tick
zoomed "Linux Terminal" && pass "a window maximized before it was framed gets a maximized frame" \
    || fail "maximized before framed: its frame $("$WINE" probe.exe rect SgLinuxWindow "Linux Terminal" | tr -d '\r')"
"$WINE" probe.exe wmclose SgLinuxWindow "Linux Terminal" > /dev/null 2>&1
i=0; while kill -0 "$XT" 2>/dev/null && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
kill -0 "$XT" 2>/dev/null && fail "WM_CLOSE to its frame did not close it" || { pass "WM_CLOSE to its frame closes it"; XT=; }
printf 'END\n' > "$T/list"   # gone from the compositor's list too
i=0; while ! "$WINE" probe.exe exists SgLinuxWindow "" | grep -q 'exists=0' && [ $i -lt 24 ]; do sleep 0.5; i=$((i + 1)); done
"$WINE" probe.exe exists SgLinuxWindow "" | grep -q 'exists=0' && pass "...and the frame goes" \
    || fail "the frame stayed: $("$WINE" probe.exe list SgLinuxWindow - 2>/dev/null | tr -d '\r' | tr '\n' ';')"
# one bigger than the work area (Firefox at the size it had maximized): a maximized frame
xterm -geometry 200x80+0+0 -title 'Big Terminal' & XT=$!
i=0; while [ -z "$(xwininfo -root -children | awk '/"Big Terminal"/ {print $1; exit}')" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
printf '%d shown - XTerm\tBig Terminal\nEND\n' "$(xwininfo -root -children | awk '/"Big Terminal"/ {print $1; exit}')" > "$T/list"
sleep 4
zoomed "Big Terminal" && pass "a window bigger than the work area gets a maximized frame" \
    || fail "bigger than the work area: its frame $("$WINE" probe.exe rect SgLinuxWindow "Big Terminal" | tr -d '\r')"
# hung: asked to close it stays; End task ends it (gone, or a zombie not yet reaped)
alive() { [ -d "/proc/$1" ] && ! grep -q '^[0-9]* (.*) Z' "/proc/$1/stat" 2>/dev/null; }
kill -STOP "$XT"
"$WINE" probe.exe wmclose SgLinuxWindow "Big Terminal" > /dev/null 2>&1; sleep 2
alive "$XT" && pass "a hung program asked to close stays (it does not answer)" || fail "the stopped xterm closed: test broken"
"$WINE" probe.exe endtask SgLinuxWindow "Big Terminal" > /dev/null 2>&1
i=0; while alive "$XT" && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
alive "$XT" && fail "End task left the hung program running" || pass "End task ends the hung program (its process killed)"
kill -9 "$XT" 2>/dev/null; wait "$XT" 2>/dev/null; XT=
# gone from the compositor's list too: the next xterm may get its X id, and a
# list still naming it had the taskbar frame the new one at once -- before
# this test found it -- and then let it go with the list naming none
printf 'END\n' > "$T/list"; sleep 1.5
# 10. a request about a window that is gone (a program closing a popup as it
# asks to be full screen): the shell stays (David's X1, 2026-10-03: the shell
# ended -- the X error went to Xlib's own handler; patches/sg/0784)
"$T/netwm-fullscreen" 0x7ffff01 1 2>/dev/null; sleep 2
if xwininfo -root -tree 2>/dev/null | grep -q '"shell - Wine Desktop"' && pgrep -f 'explorer.exe /desktop=shell' >/dev/null; then
    pass "a request about a window already gone leaves the shell running"
else fail "the shell ended after a request about a window that was gone"; fi
# 11. one opening while a Wine window is active (a Linux Terminal opened over
# PowerShell, David 2026-10-03): its frame is active and it has the keys,
# with no click -- not hidden while it was taken in and shown again behind,
# nor its frame given the X focus before the program was in it (0788)
"$WINE" probe.exe activate Notepad "" > /dev/null 2>&1; sleep 1
xterm -geometry 50x12+300+200 -bg '#3060a0' -fg white -title 'New Terminal' -e sh -c "cd '$T'; exec sh" & XT=$!
i=0; while [ -z "$(xwininfo -root -children | awk '/"New Terminal"/ {print $1; exit}')" ] && [ $i -lt 100 ]; do sleep 0.2; i=$((i + 1)); done
X3=$(xwininfo -root -children | awk '/"New Terminal"/ {print $1; exit}')
if [ -z "$X3" ]; then
    fail "the new xterm's window never showed (test broken)"
else
    fg Notepad "" || "$WINE" probe.exe activate Notepad "" > /dev/null 2>&1
    "$WINE" probe.exe fglog 15 - - > "$T/fglog" 2>/dev/null &
    i=0; while ! grep -q logging "$T/fglog" 2>/dev/null && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
    printf '%d shown - XTerm\tNew Terminal\nEND\n' "$X3" > "$T/list"
    framed "$X3" || fail "the new window was never framed"
    i=0; while ! fg SgLinuxWindow "New Terminal" && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
    sleep 2   # what the window behind would do, late
fi
xdotool mousemove 900 650; sleep 0.3
fg SgLinuxWindow "New Terminal" && pass "a Linux window opened over Notepad is the active window" \
    || fail "opened over Notepad, not active: $("$WINE" probe.exe foreground SgLinuxWindow "New Terminal" | tr -d '\r')"
# made active once: Notepad not again after it (0853)
tr -d '\r' < "$T/fglog" 2>/dev/null | sed -n '/^fg SgLinuxWindow|New Terminal/,$p' > "$T/fgafter"
[ -s "$T/fgafter" ] && ! grep -q '^fg Notepad' "$T/fgafter" \
    && pass "...and the window behind it was not made active again while it was framed" \
    || fail "the windows made active as it was framed: $(tr -d '\r' < "$T/fglog" | tr '\n' ' ')(Notepad active again after it: its program may take the keys)"
[ "$(printf '%d' "$(xdotool getwindowfocus 2>/dev/null)")" = "$(printf '%d' "$X3")" ] && pass "...and has the X focus" \
    || fail "...the X focus is on $(xdotool getwindowfocus 2>/dev/null), not $X3"
xdotool type --delay 40 'touch typed-new'; xdotool key Return; sleep 1.5
[ -e "$T/typed-new" ] && pass "...and keys reach it without a click" || fail "typed keys did not reach the new window"
kill "$XT" 2>/dev/null; XT=
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
