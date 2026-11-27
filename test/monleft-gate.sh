#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A display LEFT of the main one (GitHub #17, #18, #19; patches/sg/9020, 9021). QA VM ONLY: it
# starts a headless sg-compositor (never on a machine someone works on).
#
# Windows keeps the main display at 0,0 and a display on its left at negative coordinates; X
# counts from the leftmost display. A headless compositor with the 1920x1080 display at 0,0 and
# the main one (2560x1440, the X server's primary output) right of it at 1920,0, the shell's
# desktop over both:
#   1. a Linux program's window opened with the pointer on the main display is framed there,
#      centred -- it was framed 1920 px further right, off the screen (#17, #19's FileZilla)
#   2. one opened with the pointer on the left display is framed on that display
#   3. Win+Shift+Left moves a Wine window to the left display, same place, same size; again, round
#      to the main display (#18)
#   4. Win+Left snaps the Wine window to the main display's left half; Win+Left again to the
#      left display's right half (the facing half, as Windows goes on across)
#   5. the framed Linux window maximized fills its own display (it spanned every display: the
#      default maximized size in the shell's own process is the virtual screen); Win+Shift+Left
#      moves it, maximized, to the left display
#
#   SG_QA_VM=1 WINE=/opt/wine-sg/bin/wine test/monleft-gate.sh
#   SG_COMPOSITOR_BUILD=<dir with sg-compositor> (default: the installed one)
#   SG_PREBUILT=<dir with fake-lockctl, monleft-app, vptr, vdesk-probe.exe, linuxembed-probe.exe>:
#     a QA VM has no compilers (vptr: sg-compositor's test/vptr.c)
# Mutants: SG_MUTANT_EMBED_ROOT_COORDS (winex11.drv sg_embed.c) fails 1, 2;
#          SG_MUTANT_MOVE_NO_NEIGHBOUR (explorer shellkeys.c) fails 3, 4, 5;
#          SG_MUTANT_FRAME_MAX_ALL_DISPLAYS (explorer systray.c) fails 5.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
need="xwininfo xrandr xdotool wlr-randr"; [ -z "${SG_PREBUILT:-}" ] && need="$need cc $MINGW wayland-scanner"
for t in $need; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
COMP=${SG_COMPOSITOR_BUILD:+$SG_COMPOSITOR_BUILD/sg-compositor}
COMP=${COMP:-$(command -v sg-compositor)}
[ -x "$COMP" ] || { echo "SKIP: no sg-compositor"; exit 77; }
if [ -z "${SG_QA_VM:-}" ]; then echo "SKIP: QA VM only (SG_QA_VM=1)"; exit 77; fi
RC=0; PIDS=
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-monleft.XXXXXX); chmod 700 "$T"
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    for p in $PIDS; do pkill -P "$p" 2>/dev/null; kill "$p" 2>/dev/null; done; sleep 1
    for p in $PIDS; do kill -KILL "$p" 2>/dev/null; done
    [ -n "${KEEP:-}" ] && echo "kept $T" || rm -rf "$T"
}
trap cleanup EXIT INT TERM
if [ -n "${SG_PREBUILT:-}" ]; then
    for f in fake-lockctl monleft-app vptr vdesk-probe.exe linuxembed-probe.exe; do
        cp "$SG_PREBUILT/$f" "$T/" || { echo "FAIL  no prebuilt $f"; exit 1; }
    done
else
    CT=${SG_COMPOSITOR_SRC:-$HERE/../../sg-compositor/test}
    VPX=$CT/wlr-virtual-pointer-unstable-v1.xml
    { cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" && cc -O2 -o "$T/monleft-app" "$HERE/monleft-app.c" -lX11 &&
      wayland-scanner client-header "$VPX" "$T/wlr-virtual-pointer-unstable-v1-client-protocol.h" &&
      wayland-scanner private-code "$VPX" "$T/vp.c" &&
      cc -O2 -I"$T" -o "$T/vptr" "$CT/vptr.c" "$T/vp.c" $(pkg-config --cflags --libs wayland-client) &&
      TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 &&
      TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/linuxembed-probe.exe" "$HERE/linuxembed-probe.c"; } ||
        { echo "FAIL  the test programs did not build"; exit 1; }
fi

unset DISPLAY WAYLAND_DISPLAY XAUTHORITY SESSION_MANAGER
mkdir -p "$T/prefix" "$T/cx" "$T/run"; chmod 700 "$T/cx" "$T/run"
# every display at 96 DPI (9971 would give the 1440p one 125%): the probes are not DPI aware and
# would see the main display's coordinates scaled
export SG_LOGPIXELS=96
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER
XDG_RUNTIME_DIR="$T/cx" WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman \
    "$COMP" -L "$T/p.sock" -C "$T/c.sock" -U "$(id -u)" -- \
    sh -c "echo \$WAYLAND_DISPLAY > $T/wl; echo \$DISPLAY > $T/disp; exec sleep 1200" >>"$T/comp.log" 2>&1 &
PIDS="$PIDS $!"
i=0; while [ ! -s "$T/disp" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
DISPLAY=$(cat "$T/disp" 2>/dev/null); export DISPLAY
case "$DISPLAY" in ""|:0) echo "FAIL  no display from the compositor"; cat "$T/comp.log"; exit 1 ;; esac
# a client on the X server all along: Xwayland ends ten seconds after its last client goes, and
# the primary output set below with it
xprop -root -spy >/dev/null 2>&1 & PIDS="$PIDS $!"
WL=$(cat "$T/wl")
randr() { XDG_RUNTIME_DIR="$T/cx" WAYLAND_DISPLAY=$WL wlr-randr "$@"; }
OUTS=$(randr | awk '/^[^ ]/ {print $1}')
OM=$(echo "$OUTS" | sed -n 1p); OL=$(echo "$OUTS" | sed -n 2p)
randr --output "$OM" --custom-mode 2560x1440 --pos 1920,0 --output "$OL" --custom-mode 1920x1080 --pos 0,0 >>"$T/comp.log" 2>&1
sleep 2
xrandr --output "$OM" --primary 2>>"$T/comp.log"
# vptr talks to the privileged socket
ptr() { XDG_RUNTIME_DIR="$T/cx" WAYLAND_DISPLAY="$T/p.sock" "$T/vptr" 4480 1440 "$@" >/dev/null 2>&1 </dev/null; sleep 1; }
case "$(xwininfo -root 2>/dev/null | awk '/Width:/ {w=$2} /Height:/ {h=$2} END {print w "x" h}')" in
    4480x1440) ;;
    *) echo "SKIP: the compositor did not make the two outputs"; exit 77 ;;
esac
export XDG_RUNTIME_DIR="$T/run"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d x11 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v HostMonitors /d Y /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 4480x1440 /f >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/vdesk-probe.exe" "$T/linuxembed-probe.exe" "$WINEPREFIX/drive_c/"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T"
printf 'END\n' > "$T/list"
"$WINE" explorer /desktop=shell,4480x1440 > "$T/explorer.out" 2>&1 & PIDS="$PIDS $!"
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 4
# the keyboard to the session's desktop, as the taskbar brings a window forward (XACTIVATE): a
# headless compositor has had no click to give it the focus; the keys then come through X (xdotool)
ctl() { python3 -c '
import socket, sys
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); s.sendall(sys.argv[2].encode() + b"\n")
out = b""
while True:
    d = s.recv(4096)
    if not d: break
    out += d
print(out.decode().strip())' "$T/c.sock" "$1"; }
dw=$(ctl XWINDOWS | awk -F'\t' '$2 ~ / Desktop/ { split($1, a, " "); print a[1] }' | head -n 1)
[ -n "$dw" ] && ctl "XACTIVATE $dw" >/dev/null
cd "$WINEPREFIX/drive_c"
lrect() { "$WINE" linuxembed-probe.exe rect SgLinuxWindow "$1" 2>/dev/null | tr -d '\r'; }
wrect() { "$WINE" vdesk-probe.exe rect "$1" 2>/dev/null | tr -d '\r' | sed 's/^rect=//; s/,/ /g'; }
K() { xdotool key "$@"; sleep 2; }
# frame NAME: map the program's window (pointer already placed), list it for the taskbar to frame
frame() {
    "$T/monleft-app" "$1" "$T/app.log" 600 & PIDS="$PIDS $!"
    i=0; while ! grep -q "^$1 " "$T/app.log" 2>/dev/null && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    sleep 1
    awk '{printf "%d shown - monleft\t%s\n", $2, $1} END {print "END"}' "$T/app.log" > "$T/list"
    i=0; while ! lrect "$1" | grep -q '^-\?[0-9]' && [ $i -lt 30 ]; do sleep 0.5; i=$((i + 1)); done
    sleep 2
}
# inside L T R B  RECT (four numbers): RECT whole within the box
inside() { [ "$5" -ge "$1" ] && [ "$6" -ge "$2" ] && [ "$7" -le "$3" ] && [ "$8" -le "$4" ]; }

# 1. pointer on the main display
ptr m 3200 700
frame MainTerm
r1=$(lrect MainTerm)
# shellcheck disable=SC2086
if [ -n "$r1" ] && inside 0 0 2560 1440 $r1; then
    set -- $r1; c=$((($1 + $3) / 2))
    [ "$c" -ge 1240 ] && [ "$c" -le 1320 ] && pass "a Linux window opened on the main display is framed there, centred ($r1)" ||
        fail "a Linux window on the main display: framed at '$r1', not centred (middle $c, want ~1280)"
else fail "a Linux window opened on the main display: framed at '$r1', want inside 0,0-2560,1440 (Windows' coordinates)"; fi
# 2. pointer on the left display
ptr m 900 500
frame LeftTerm
r2=$(lrect LeftTerm)
# shellcheck disable=SC2086
[ -n "$r2" ] && inside -1920 0 0 1080 $r2 && pass "a Linux window opened on the left display is framed there ($r2)" ||
    fail "a Linux window opened on the left display: framed at '$r2', want inside -1920,0-0,1080"

# 3. a Wine window on the main display, Win+Shift+Left
"$WINE" vdesk-probe.exe window Alpha 600 300 >/dev/null 2>&1 &
sleep 4
"$WINE" linuxembed-probe.exe activate SgVdeskProbe Alpha >/dev/null 2>&1; sleep 1
a0=$(wrect Alpha | cut -d' ' -f1-4)
K super+shift+Left; a1=$(wrect Alpha | cut -d' ' -f1-4)
K super+shift+Left; a2=$(wrect Alpha | cut -d' ' -f1-4)
echo "      Alpha: $a0 -> $a1 -> $a2"
# shellcheck disable=SC2086
{ [ -n "$a1" ] && inside -1920 0 0 1080 $a1 && set -- $a0 && w0=$(($3 - $1)) && set -- $a1 && [ $(($3 - $1)) = "$w0" ]; } &&
    pass "Win+Shift+Left: the Wine window on the left display, at its size ($a0 -> $a1)" ||
    fail "Win+Shift+Left: the Wine window at '$a1' (was '$a0'), want on the left display -1920..0 at its size"
# shellcheck disable=SC2086
[ -n "$a2" ] && [ "$a1" != "$a0" ] && inside 0 0 2560 1440 $a2 && pass "Win+Shift+Left again: round to the main display ($a2)" ||
    fail "Win+Shift+Left from the leftmost display: at '$a2', want round on the main display"

# 4. Win+Left twice: the main display's left half, then the left display's right half
K super+Left; s1=$(wrect Alpha | cut -d' ' -f1-4)
K super+Left; s2=$(wrect Alpha | cut -d' ' -f1-4)
echo "      Alpha snapped: $s1 -> $s2"
# shellcheck disable=SC2086
{ [ -n "$s1" ] && set -- $s1 && [ "$1" = 0 ] && [ "$3" = 1280 ]; } && pass "Win+Left: the main display's left half ($s1)" ||
    fail "Win+Left: '$s1', want the main display's left half 0..1280"
# shellcheck disable=SC2086
{ [ -n "$s2" ] && set -- $s2 && [ "$1" = -960 ] && [ "$3" = 0 ]; } &&
    pass "Win+Left again: the left display's right half ($s2)" ||
    fail "Win+Left again: '$s2', want the left display's right half -960..0"
# 5. the framed Linux window (a click into it brings it forward), maximized: on its display only;
#    Win+Shift+Left: maximized on the left display
r=$(lrect MainTerm); set -- $r
xdotool mousemove $((($1 + $3) / 2 + 1920)) $((($2 + $4) / 2)) click 1; sleep 1
"$WINE" linuxembed-probe.exe maximize SgLinuxWindow MainTerm >/dev/null 2>&1; sleep 2
r3=$(lrect MainTerm)
# shellcheck disable=SC2086
{ [ -n "$r3" ] && set -- $r3 && [ "$1" -ge -16 ] && [ "$1" -le 0 ] && [ "$3" -ge 2560 ] && [ "$3" -le 2576 ]; } &&
    pass "the framed Linux window maximized: its own display's work area ($r3)" ||
    fail "the framed Linux window maximized: '$r3', want the main display 0..2560 (not every display)"
K super+shift+Left
r4=$(lrect MainTerm); z4=$("$WINE" linuxembed-probe.exe zoomed SgLinuxWindow MainTerm 2>/dev/null | tr -d '\r')
# shellcheck disable=SC2086
{ [ -n "$r4" ] && set -- $r4 && [ "$1" -lt -1900 ] && [ "$3" -le 16 ] && [ "$3" -ge 0 ] && [ "$z4" = zoomed=1 ]; } &&
    pass "Win+Shift+Left: the maximized Linux window maximized on the left display ($r4, $z4)" ||
    fail "Win+Shift+Left on the maximized Linux window: '$r4' $z4, want maximized on -1920..0"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
