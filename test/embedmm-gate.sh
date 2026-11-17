#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program's full-screen windows on two monitors are each full screen on their own
# monitor (7826). sdl-freerdp3 /multimon (Remote Desktop Connection, Devolutions RDM's "Open
# (external)" with "Use multiple monitors") makes one window per monitor, each fixed at its
# monitor's size and place: the first full screen as it is mapped, the second just so (no request,
# or not in time: a window that cannot be resized, exactly a monitor, which X window managers take
# for full screen). Framed,
# the first was a maximized pop-up over the whole desktop (2560 wide), the second got a caption
# and border, did not fit the work area and was maximized across both monitors.
# A headless sg-compositor with two outputs (1280x720 side by side), the shell's desktop over both,
# a stand-in client (embedmm-app.c): each frame must be exactly its monitor, without the frame's
# border or caption, and each window full screen (_NET_WM_STATE); and neither display's taskbar
# over the program in the foreground (the second display's bar stayed over the session).
#   WINE=/opt/wine-sg/bin/wine SG_COMPOSITOR_BUILD=<dir with sg-compositor> test/embedmm-gate.sh
#   (mutants SG_MUTANT_EMBED_BORDERLESS_WINDOWED, SG_MUTANT_EMBED_FS_MAXIMIZED, winex11.drv sg_embed.c;
#    SG_MUTANT_SECOND_BAR_OVER_FULLSCREEN, explorer systray.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
COMP="${SG_COMPOSITOR_BUILD:-}/sg-compositor"
for t in xwininfo xprop cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -x "$COMP" ] || { echo "SKIP: no sg-compositor (SG_COMPOSITOR_BUILD)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-embedmm.XXXXXX); chmod 700 "$T"; mkdir -m 700 "$T/cx" "$T/run"
PIDS=
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    for p in $PIDS; do kill "$p" 2>/dev/null; done; sleep 1
    for p in $PIDS; do kill -KILL "$p" 2>/dev/null; done   # the compositor waits on its child otherwise
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/app" "$HERE/embedmm-app.c" -lX11 || { echo "FAIL  embedmm-app did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
"$MINGW" -O2 -o "$T/bars.exe" "$HERE/embedmm-bars.c" || { echo "FAIL  bars probe did not build"; exit 1; }
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY SESSION_MANAGER
XDG_RUNTIME_DIR="$T/cx" WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman \
    "$COMP" -L "$T/p.sock" -C "$T/c.sock" -U "$(id -u)" -- sh -c "echo \$DISPLAY > $T/disp; exec sleep 900" > "$T/comp.log" 2>&1 &
PIDS="$PIDS $!"
i=0; while [ ! -s "$T/disp" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
DISPLAY=$(cat "$T/disp" 2>/dev/null); export DISPLAY
[ -n "$DISPLAY" ] && [ "$DISPLAY" != ":0" ] || { echo "FAIL  no display from the compositor"; exit 1; }
sleep 1
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER
export WAYLAND_DISPLAY=sg-no-wayland
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$T/bars.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d x11 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v HostMonitors /d Y /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 2560x720 /f >/dev/null 2>&1
"$WINESERVER" -w
printf 'END\n' > "$T/list"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
WINEDEBUG="${EMBED_DEBUG:--all}" "$WINE" explorer /desktop=shell,2560x720 > "$T/explorer.out" 2>&1 & PIDS="$PIDS $!"
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 4
# the client: both windows made and mapped before they are listed (framed)
"$T/app" 1280 720 40 "$T/app.log" & PIDS="$PIDS $!"
i=0; while [ "$(cat "$T/app.log" 2>/dev/null | grep -c '^[AB] ')" -lt 2 ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
A=$(awk '$1 == "A" {print $2}' "$T/app.log"); B=$(awk '$1 == "B" {print $2}' "$T/app.log")
sleep 1
printf '%d shown - mmA\tmmA\n%d shown - mmB\tmmB\nEND\n' "$A" "$B" > "$T/list"
sleep 8
cd "$WINEPREFIX/drive_c"
ra=$("$WINE" probe.exe rect SgLinuxWindow mmA 2>/dev/null | tr -d '\r')
rb=$("$WINE" probe.exe rect SgLinuxWindow mmB 2>/dev/null | tr -d '\r')
[ "$ra" = "0 0 1280 720" ] && pass "the first window's frame is its monitor exactly ($ra)" || fail "the first window's frame: '$ra' (wanted 0 0 1280 720)"
[ "$rb" = "1280 0 2560 720" ] && pass "the second window's frame is its monitor exactly ($rb)" || fail "the second window's frame: '$rb' (wanted 1280 0 2560 720)"
for x in A B; do
    id=$(awk -v w=$x '$1 == w {print $2}' "$T/app.log")
    st=$(xprop -id "$id" _NET_WM_STATE 2>/dev/null)
    case "$st" in *_NET_WM_STATE_FULLSCREEN*) pass "window $x is full screen ($st)" ;; *) fail "window $x is not full screen: $st" ;; esac
done
# the taskbars: the program in the foreground is full screen on both displays, so neither bar is
# over it (the second display's bar stayed over the session, always above everything)
bars=$("$WINE" bars.exe 2>/dev/null | tr -d '\r')
printf '%s\n' "$bars" | sed 's/^/      /'
for c in Shell_TrayWnd Shell_SecondaryTrayWnd; do
    l=$(printf '%s\n' "$bars" | grep "^BAR $c ")
    case "$l" in *"topmost=0 below=1"*) pass "$c is behind the full-screen window on its display" ;;
                 "") fail "no $c" ;; *) fail "$c over the full-screen window: $l" ;; esac
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
